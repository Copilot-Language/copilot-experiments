# wcv-monitor

1. Generate monitor using the SBV backend to Copilot. E.g.:
  ```hs
  import Copilot.Language.Reify
  import Copilot.Compile.SBV
  spec = ...
  main = reify spec >>= compile defaultParams
  ```

2. Move the generated C source to `$WCV_ROOT/listener` (where `$WCV_ROOT` is the directory containing this README):
  ```
  $ mv copilot-sbv-codegen $WCV_ROOT/listener
  ```

3. Build the project:
  ```
  $ make
  ```
