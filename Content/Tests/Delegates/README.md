# Delegate lifecycle fixtures

Editor proof for `angelscript/feature-delegates-ue-interop` task 4.1 lives in
`Angelscript.UnitTest.NativeEngine.Compile.DelegateLifecycle`.

The dependent Blueprint is created after `CompileModules` publishes the script
`UCLASS` and `UDelegateFunction` (`FKismetEditorUtilities::CreateBlueprint` on
the generated listener). No cooked `.uasset` is required for that prefix.

Cooked-load, when assets are added here, uses the 1.1 route:

```powershell
Invoke-Harness -Command ue.commandlet -Context $context -Parameters @{
    Commandlet = 'Cook'
    ExtraArguments = @('-TargetPlatform=Windows')
    TimeoutMs = 3600000
}
```
