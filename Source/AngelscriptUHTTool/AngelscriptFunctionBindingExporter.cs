using System;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace AngelscriptUHTTool;

[UnrealHeaderTool]
internal static class AngelscriptFunctionBindingExporter
{
	[UhtExporter(
		Name = "AngelscriptFunctionBinding",
		Description = "Exports Angelscript function binding data",
		Options = UhtExporterOptions.Default | UhtExporterOptions.CompileOutput,
		CppFilters = ["AS_FunctionBinding_*_NativeModuleFunctionAddress_*.cpp"],
		OtherFilters = ["AS_FunctionBinding_*.inc"],
		ModuleName = "AngelscriptRuntime")]
	private static void Export(IUhtExportFactory factory)
	{
		int generatedFileCount = AngelscriptFunctionBindingCodeGenerator.Generate(factory);
		Console.WriteLine("AngelscriptUHTTool function binding exporter wrote {0} generated files.", generatedFileCount);
	}

	internal static bool IsAngelscriptCallable(UhtFunction function)
	{
		return function.FunctionType == UhtFunctionType.Function &&
			(function.FunctionFlags.HasAnyFlags(EFunctionFlags.BlueprintCallable | EFunctionFlags.BlueprintPure) ||
			function.MetaData.ContainsKey("ScriptCallable"));
	}
}
