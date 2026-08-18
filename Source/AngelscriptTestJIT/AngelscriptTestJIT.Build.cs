using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptTestJIT : ModuleRules
	{
		public AngelscriptTestJIT(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			// Each generated AS module owns one independent .jit.cpp translation
			// unit. Their module-local layout declarations are intentionally not
			// safe to merge into a UBT unity source.
			bUseUnity = false;
			PrivateDefinitions.Add("AS_ENABLE_EDITOR_JITTED_CODE=1");
			PrivateIncludePaths.Add(ModuleDirectory);

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"AngelscriptRuntime",
			});
		}
	}
}
