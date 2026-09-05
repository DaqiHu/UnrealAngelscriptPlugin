using System.IO;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class AngelscriptTest : ModuleRules
	{
		public AngelscriptTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			bUseUnity = true;
			PrivateDefinitions.Add("AS_ENABLE_EDITOR_JITTED_CODE=1");
			bool bCompileLegacyTests = AngelscriptBuildSettings.ShouldCompileUnitTests(Target, this);
			bool bCompileReplacementTests = AngelscriptBuildSettings.ShouldCompileTests(Target, this);

			PublicIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(ModuleDirectory);

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AngelscriptRuntime",
			});

			if (bCompileLegacyTests)
			{
				ForceIncludeFiles.Add(Path.Combine(ModuleDirectory, "Legacy", "AngelscriptCQTest.h"));

				// Retained only beside the quarantined source as reconstruction context.
				// The default hard gate is false and .ubtignore markers keep the legacy
				// corpus outside active UBT/UHT discovery.
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Core"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Debugger"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Dump"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "AngelScriptSDK"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Preprocessor"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Generator"));
				PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy", "Shared"));
				PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(
					ModuleDirectory, "..", "AngelscriptTestJIT")));

				PublicDependencyModuleNames.AddRange(new string[]
				{
					"GameplayTags",
					"Json",
					"JsonUtilities",
					"PropertyBindingUtils",
				});

				PrivateDependencyModuleNames.AddRange(new string[]
				{
					"AIModule",
					"EnhancedInput",
					"InputCore",
					"Slate",
					"SlateCore",
					"UMG",
				});

				if (Target.bBuildEditor)
				{
					PrivateDependencyModuleNames.AddRange(new string[]
					{
						"BlueprintGraph",
						"CQTest",
						"LevelEditor",
						"Networking",
						"Sockets",
						"UnrealEd",
						"AngelscriptEditor",
						"AngelscriptTestJIT",
					});
				}
			}

			if (bCompileReplacementTests)
			{
				// Replacement frontend tests deserialize stage dumps independently.
				PrivateDependencyModuleNames.Add("Json");
			}

			if (Target.bBuildEditor
				&& bCompileReplacementTests
				&& !PrivateDependencyModuleNames.Contains("CQTest"))
			{
				PrivateDependencyModuleNames.Add("CQTest");
			}
		}
	}
}
