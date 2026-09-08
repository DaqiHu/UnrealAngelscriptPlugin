cmake_minimum_required(VERSION 3.25)

foreach(RequiredVariable IN ITEMS AS_STANDALONE_EXE AS_STANDALONE_FIXTURE_ROOT AS_STANDALONE_TEST_ROOT)
	if(NOT DEFINED ${RequiredVariable} OR "${${RequiredVariable}}" STREQUAL "")
		message(FATAL_ERROR "${RequiredVariable} is required")
	endif()
endforeach()

function(run_cli ExpectedExit)
	execute_process(
		COMMAND "${AS_STANDALONE_EXE}" ${ARGN}
		RESULT_VARIABLE ActualExit
		OUTPUT_VARIABLE StandardOutput
		ERROR_VARIABLE StandardError
	)
	if(NOT ActualExit EQUAL ExpectedExit)
		message(FATAL_ERROR
			"CLI exit mismatch: expected ${ExpectedExit}, got ${ActualExit}\n"
			"stdout:\n${StandardOutput}\n"
			"stderr:\n${StandardError}")
	endif()
	set(LAST_STDOUT "${StandardOutput}" PARENT_SCOPE)
	set(LAST_STDERR "${StandardError}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${AS_STANDALONE_TEST_ROOT}")
file(MAKE_DIRECTORY "${AS_STANDALONE_TEST_ROOT}")

run_cli(0 --version)
if(NOT LAST_STDOUT MATCHES "^Unreal AngelScript 1\\.0\\.0")
	message(FATAL_ERROR "--version must lead with the product version: ${LAST_STDOUT}")
endif()
if(NOT LAST_STDOUT MATCHES "AngelScript 2\\.33\\.0 WIP lineage")
	message(FATAL_ERROR "--version must report upstream lineage separately: ${LAST_STDOUT}")
endif()

run_cli(0 --help)
if(NOT LAST_STDOUT MATCHES "compile --dialect native\\|ue")
	message(FATAL_ERROR "--help is missing compile profiles: ${LAST_STDOUT}")
endif()
if(LAST_STDOUT MATCHES "as-standalone run")
	message(FATAL_ERROR "--help must not expose a UE run command: ${LAST_STDOUT}")
endif()

set(CompileOutput "${AS_STANDALONE_TEST_ROOT}/compile")
run_cli(
	0
	compile
	--dialect native
	--script-root "${AS_STANDALONE_FIXTURE_ROOT}"
	--entry main.as
	--output "${CompileOutput}"
	--diagnostics json
	--emit-bytecode
)
foreach(ExpectedPath IN ITEMS
	"${CompileOutput}/result.json"
	"${CompileOutput}/diagnostics.jsonl")
	if(NOT EXISTS "${ExpectedPath}")
		message(FATAL_ERROR "compile did not publish ${ExpectedPath}")
	endif()
endforeach()
file(READ "${CompileOutput}/result.json" CompileResult)
if(NOT CompileResult MATCHES "\"status\": \"complete\"")
	message(FATAL_ERROR "compile result is not complete: ${CompileResult}")
endif()
file(GLOB ByteCodeFiles "${CompileOutput}/modules/*.asbc")
list(LENGTH ByteCodeFiles ByteCodeCount)
if(NOT ByteCodeCount EQUAL 1)
	message(FATAL_ERROR "compile must publish exactly one bytecode file")
endif()

run_cli(
	0
	compile
	--dialect native
	--script-root "${AS_STANDALONE_FIXTURE_ROOT}"
	--entry conditional.as
	--output "${AS_STANDALONE_TEST_ROOT}/conditional"
)

set(UnknownConditionOutput "${AS_STANDALONE_TEST_ROOT}/unknown-condition")
run_cli(
	1
	compile
	--dialect native
	--script-root "${AS_STANDALONE_FIXTURE_ROOT}"
	--entry unknown-condition.as
	--output "${UnknownConditionOutput}"
	--diagnostics json
)
file(READ "${UnknownConditionOutput}/diagnostics.jsonl" UnknownConditionDiagnostics)
if(NOT UnknownConditionDiagnostics MATCHES "ASL-PREPROCESS-UNKNOWN-FLAG")
	message(FATAL_ERROR
		"LanguageCore diagnostic did not reach standalone artifacts: "
		"${UnknownConditionDiagnostics}")
endif()

set(InvalidOutput "${AS_STANDALONE_TEST_ROOT}/invalid")
run_cli(
	1
	compile
	--dialect native
	--script-root "${AS_STANDALONE_FIXTURE_ROOT}"
	--entry invalid.as
	--output "${InvalidOutput}"
	--diagnostics text
	--emit-bytecode
)
file(READ "${InvalidOutput}/result.json" InvalidResult)
if(NOT InvalidResult MATCHES "\"status\": \"failed\"")
	message(FATAL_ERROR "invalid source must publish a failed result: ${InvalidResult}")
endif()

run_cli(
	2
	run
	--script-root "${AS_STANDALONE_FIXTURE_ROOT}"
	--entry main.as
	--bundle forbidden
)

if(NOT LAST_STDERR MATCHES "AS-NATIVE-UNSUPPORTED-LIBRARY")
	message(FATAL_ERROR "removed execution must reject before invocation: ${LAST_STDERR}")
endif()
