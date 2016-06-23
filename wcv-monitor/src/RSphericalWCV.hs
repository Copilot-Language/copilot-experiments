{-# OPTIONS_GHC -fno-warn-name-shadowing -fno-warn-unused-do-bind #-}

-- | A version of WCV with input in spherical coordinates.
module RSphericalWCV where

import Prelude ()

import Copilot.Language
import Copilot.Language.Reify
import Copilot.Theorem
import Copilot.Theorem.Prover.Z3
import qualified Copilot.Compile.SBV as S

import qualified Copilot.Language.Operators.Propositional as P

type Vect2 = (Stream Double, Stream Double)
type Vect3 = (Stream Double, Stream Double, Stream Double)

-- | Units (conversions to SI).
deg, ft, sec, min, knot :: Stream Double
deg  = constant (pi/180)
ft   = constant 0.3048
sec  = constant 1
min  = constant 60
knot = constant (1852/3600)

-- | Ownship: lat, lon, track (rad); ground speed, vert speed (m/s); altitude (m).
latO, lonO, gsO, trkO, vsO, altO :: Stream Double
latO = extern "latO" Nothing * deg
lonO = extern "lonO" Nothing * deg
altO = extern "altO" Nothing * ft
trkO = extern "trkO" Nothing * deg
gsO  = extern "gsO"  Nothing * knot
vsO  = extern "vsO"  Nothing * ft/min

-- | Intruder: lat, lon, track  (rad); ground speed, vert speed (m/s); altitude (m).
latI, lonI, gsI, trkI, vsI, altI :: Stream Double
latI = extern "latI" Nothing * deg
lonI = extern "lonI" Nothing * deg
altI = extern "altI" Nothing * ft
trkI = extern "trkI" Nothing * deg
gsI  = extern "gsI"  Nothing * knot
vsI  = extern "vsI"  Nothing * ft/min

-- | Thresholds: dthr (m), zthr (m), tthr (s), tcoathr (s).
dthr, tthr, zthr, tcoathr :: Stream Double
dthr    = extern "dthr"    Nothing * ft
zthr    = extern "zthr"    Nothing * ft
tthr    = extern "tthr"    Nothing * sec
tcoathr = extern "tcoathr" Nothing * sec

-- | Taylor series approximations of sin and cos.
sin', cos' :: Stream Double -> Stream Double
sin' = flip local $ \x -> x - (pow3 x)/6 + (pow5 x)/120 - (pow7 x)/5040 + (pow9 x)/362880 - (pow11 x)/39916800 + (pow13 x)/6227020800
cos' = flip local $ \x -> 1 - (pow2 x)/2 + (pow4 x)/24  - (pow6 x)/720  + (pow8 x)/40320  - (pow10 x)/3628800  + (pow12 x)/479001600
-- sin' x' = local x' $ \x -> x - (x ** 3)/6 + (x ** 5)/120 - (x ** 7)/5040 + (x ** 9)/362880 - (x ** 11)/39916800 + (x ** 13)/6227020800
-- cos' x' = local x' $ \x -> 1 - (x ** 2)/2 + (x ** 4)/24  - (x ** 6)/720  + (x ** 8)/40320  - (x ** 10)/3628800  + (x ** 12)/479001600

pow2, pow3, pow4, pow5, pow6, pow7, pow8, pow9, pow10, pow11, pow12, pow13 :: Stream Double -> Stream Double
pow2 x = x * x
pow3 x = pow2 x * x
pow4 x = pow3 x * x
pow5 x = pow4 x * x
pow6 x = pow5 x * x
pow7 x = pow6 x * x
pow8 x = pow7 x * x
pow9 x = pow8 x * x
pow10 x = pow9 x * x
pow11 x = pow10 x * x
pow12 x = pow11 x * x
pow13 x = pow12 x * x

spherical2xyz :: Stream Double -> Stream Double -> Vect3
spherical2xyz lat lon = (label "?x" x, label "?y" y, label "?z" z)
      where r     = 6371000 -- Radius of the earth in meters
            theta = pi/2 - lat -- Convert latitude to 0-pi
            phi   = pi - lon
            x     = r * sin' theta * cos' phi
            y     = r * sin' theta * sin' phi
            z     = r * cos' theta

dot3 :: Vect3 -> Vect3 -> Stream Double
dot3 (x1, y1, z1) (x2, y2, z2) = x1 * x2 + y1 * y2 + z1 * z2

norm3 :: Vect3 -> Stream Double
norm3 v = sqrt (v `dot3` v)

vect3_orthog_toy :: Vect3 -> Vect3
vect3_orthog_toy (x, y, _) = (mux (x /= 0 || y /= 0) y 1, mux (x /= 0 || y /= 0) (-x) 0, constD 0)

cross3 :: Vect3 -> Vect3 -> Vect3
cross3 (x1, y1, z1) (x2, y2, z2) = (y1 * z2 - z1 * y2, z1 * x2 - x1 * z2, x1 * y2 - y1 * x2)

vect3_orthog_toz :: Vect3 -> Vect3
vect3_orthog_toz v = v `cross3` vect3_orthog_toy v

unit :: Vect3 -> Vect3
unit (x, y, z) = (x / n, y / n, z / n)
      where n = norm3 (x, y, z)

vect3_orthonorm_toy :: Vect3 -> Vect3
vect3_orthonorm_toy = unit . vect3_orthog_toy

vect3_orthonorm_toz :: Vect3 -> Vect3
vect3_orthonorm_toz = unit . vect3_orthog_toz

sphere_to_2D_plane :: Vect3 -> Vect3 -> Vect2
sphere_to_2D_plane nzv w = (ymult `dot3` w, - (zmult `dot3` w))
      where ymult = vect3_orthonorm_toy nzv
            zmult = vect3_orthonorm_toz nzv

pO :: Vect3
pO = spherical2xyz latO lonO

pI :: Vect3
pI = spherical2xyz latI lonI

sOx, sOy, sOz :: Stream Double
(sOx, sOy, sOz) = (0, 0, altO)

sIx, sIy, sIz :: Stream Double
(sIx, sIy, sIz) = (sI2x, sI2y, altI)
      where (sI2x, sI2y) = sphere_to_2D_plane pO pI

vOx, vOy, vOz :: Stream Double
(vOx, vOy, vOz) = (gsO * sin' trkO, gsO * cos' trkO, vsO)

vIx, vIy, vIz :: Stream Double
(vIx, vIy, vIz) = (gsI * sin' trkI, gsI * cos' trkI, vsI)

--------------------------------
-- latI velocity/position --
--------------------------------

vx, vy, vz :: Stream Double
(vx, vy, vz) = (vOx - vIx, vOy - vIy, vOz - vIz)

v :: Vect2
v = (vx, vy)

sx, sy, sz :: Stream Double
(sx, sy, sz) = (sOx - sIx, sOy - sIy, sOz - sIz)

s :: Vect2
s = (sx, sy)

------------------
-- Vector stuff --
------------------

(|*|) :: Vect2 -> Vect2 -> Stream Double
(|*|) (x1, y1) (x2, y2) = (x1 * x2) + (y1 * y2)

sq :: Vect2 -> Stream Double
sq x = x |*| x

norm :: Vect2 -> Stream Double
norm = sqrt . sq

det :: Vect2 -> Vect2 -> Stream Double
det (x1, y1) (x2, y2) = (x1 * y2) - (x2 * y1)

(~=) :: Stream Double -> Stream Double -> Stream Bool
a ~= b = abs (a - b) < 0.000001

neg :: Vect2 -> Vect2
neg (x, y) = (negate x, negate y)

--------------------
-- Time variables --
--------------------

tau :: Vect2 -> Vect2 -> Stream Double
tau s v = mux (s |*| v < 0) ((-(sq s)) / (s |*| v)) (-1)

tcpa :: Vect2 -> Vect2 -> Stream Double
tcpa s v@(vx, vy) = mux (vx ~= 0 && vy ~= 0) 0 (-(s |*| v) / sq v)

taumod :: Vect2 -> Vect2 -> Stream Double
taumod s v = mux (s |*| v < 0) ((dthr * dthr - sq s)/(s |*| v)) (-1)

tep :: Vect2 -> Vect2 -> Stream Double
tep s v = mux ((s |*| v < 0) && (delta s v dthr >= 0))
  (theta s v dthr (-1))
  (-1)

delta :: Vect2 -> Vect2 -> Stream Double -> Stream Double
delta s v d = d * d * sq v - (det s v * det s v)
-- Here the formula says: (s . orth v)^2 which is the same as det(s,v)^2

theta :: Vect2 -> Vect2 -> Stream Double -> Stream Double -> Stream Double
theta s v d e = (-(s |*| v) + e * sqrt (delta s v d)) / sq v

--------------------------
-- Some tools for times --
--------------------------

tcoa :: Stream Double -> Stream Double -> Stream Double
tcoa sz' vz' = local sz' $ \sz -> local vz' $ \vz -> mux ((sz * vz) < 0) ((-sz) / vz) (-1)

dcpa :: Vect2 -> Vect2 -> Stream Double
dcpa s@(sx, sy) v@(vx, vy) = norm (sx + tcpa s v * vx, sy + tcpa s v * vy)

--------------------------
-- Well clear Violation --
--------------------------

wcv :: (Vect2 -> Vect2 -> Stream Double) ->
       Vect2 -> Stream Double ->
       Vect2 -> Stream Double ->
       Stream Bool
wcv tvar s sz v vz = horizontalWCV tvar s v && verticalWCV sz vz

verticalWCV :: Stream Double -> Stream Double -> Stream Bool
verticalWCV sz' vz' = local sz' $ \sz -> local vz' $ \vz -> (abs sz <= zthr) || (0 <= tcoa sz vz && tcoa sz vz <= tcoathr)

horizontalWCV :: (Vect2 -> Vect2 -> Stream Double) -> Vect2 -> Vect2 -> Stream Bool
horizontalWCV tvar s v = (norm s <= dthr) || ((dcpa s v <= dthr) && (0 <= tvar s v) && (tvar s v <= tthr))

spec :: Spec
spec = do
      -- trigger "alert_wcv_tau" (wcv tau s sz v vz) []
      -- trigger "alert_wcv_tcpa" (wcv tcpa s sz v vz) []
      trigger "alert_wcv_taumod" (wcv taumod s sz v vz) []
      -- trigger "alert_wcv_tep" (wcv tep s sz v vz) []
      -- trigger "alert_wcv_vertical" (verticalWCV sz vz) []

main = reify spec >>= S.proofACSL S.defaultParams
