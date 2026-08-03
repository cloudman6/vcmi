include(GoogleTest)
include(CheckCXXCompilerFlag)

set(googleTest_Dir ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
if(EXISTS ${googleTest_Dir})
	set(GTestSrc ${googleTest_Dir}/googletest)
	set(GMockSrc ${googleTest_Dir}/googlemock)
else()
	message(FATAL_ERROR "No googletest src dir found!")
endif()

set(test_SRCS
 		StdInc.cpp
 		main.cpp
 		CMemoryBufferTest.cpp
 		CVcmiTestConfig.cpp
 		JsonComparer.cpp

 		battle/BattleHexTest.cpp
 		battle/CBattleInfoCallbackTest.cpp
 		battle/CHealthTest.cpp
		battle/CUnitStateTest.cpp
		battle/CUnitStateMagicTest.cpp
		battle/battle_UnitTest.cpp

		bonus/BonusSystemTest.cpp

		entity/CArtifactTest.cpp
		entity/CCreatureTest.cpp
		entity/CFactionTest.cpp
		entity/CHeroClassTest.cpp
		entity/CHeroTest.cpp
		entity/CSkillTest.cpp

		events/ApplyDamageTest.cpp
		events/EventBusTest.cpp

		game/BattleSpellCastTest.cpp
		game/HeroRecruitmentTest.cpp
		game/HeroSecondarySkillsTest.cpp

		json/JsonTests.cpp

		map/CMapEditManagerTest.cpp
		map/CMapFormatTest.cpp
		map/CGArtifactRegressionTest.cpp
		map/MapComparer.cpp
		map/TinyH3MBuilderTest.cpp

		quest/QuestTest.cpp
		quest/QuestAssertions.cpp
		quest/QuestScenarios.cpp
		quest/CQuestLoaderTest.cpp
		quest/CQuestSeerTest.cpp
		quest/CQuestSeerMultiTest.cpp
		quest/CQuestVisibilityTest.cpp
		quest/CQuestMarkerTest.cpp
		quest/CQuestGuardTest.cpp
		quest/CQuestBorderTest.cpp
		quest/CQuestGateTest.cpp

		netpacks/NetPackFixture.cpp

#TODO		scripting/BindingsCoverageTest.cpp

		spells/AbilityCasterTest.cpp
		spells/AdventureSpellEffectTest.cpp
		spells/CSpellTest.cpp
 		spells/TargetConditionTest.cpp

		spells/effects/EffectFixture.cpp
 		spells/effects/CatapultTest.cpp
 		spells/effects/CloneTest.cpp
 		spells/effects/DamageTest.cpp
 		spells/effects/DispelTest.cpp
 		spells/effects/HealTest.cpp
 		spells/effects/DemonSummonTest.cpp
		spells/effects/MoatTest.cpp
		spells/effects/ObstacleTest.cpp
		spells/effects/RemoveObstacleTest.cpp
		spells/effects/SacrificeTest.cpp
 		spells/effects/SummonTest.cpp
 		spells/effects/TeleportTest.cpp
 		spells/effects/TimedTest.cpp

 		spells/targetConditions/AbsoluteSpellConditionTest.cpp
 		spells/targetConditions/AbsoluteLevelConditionTest.cpp
 		spells/targetConditions/BonusConditionTest.cpp
 		spells/targetConditions/CreatureConditionTest.cpp
 		spells/targetConditions/ElementalConditionTest.cpp
 		spells/targetConditions/HealthValueConditionTest.cpp
 		spells/targetConditions/ImmunityNegationConditionTest.cpp
 		spells/targetConditions/NormalLevelConditionTest.cpp
 		spells/targetConditions/NormalSpellConditionTest.cpp
 		spells/targetConditions/ReceptiveFeatureConditionTest.cpp
		spells/targetConditions/ResistanceConditionTest.cpp
 		spells/targetConditions/SpellEffectConditionTest.cpp
 		spells/targetConditions/TargetConditionItemFixture.cpp

		server/queries/QueriesProcessorTest.cpp

		mock/BattleFake.cpp
		mock/mock_IGameEventCallback.cpp
 		mock/mock_MapService.cpp
 		mock/mock_MapServiceTinyH3M.cpp
 		mock/mock_BonusBearer.cpp
		mock/mock_CPSICallback.cpp
		mock/TinyH3MWriter.cpp
		mock/TinyH3MBuilder.cpp
)

set(test_HEADERS
 		StdInc.h

 		CVcmiTestConfig.h
		JsonComparer.h

		map/MapComparer.h

 		netpacks/NetPackFixture.h

		spells/effects/EffectFixture.h

		spells/targetConditions/TargetConditionItemFixture.h

		mock/BattleFake.h
		mock/mock_BonusBearer.h
		mock/mock_CStack.h
		mock/mock_IGameEventCallback.h
 		mock/mock_MapService.h
 		mock/mock_MapServiceTinyH3M.h
		mock/mock_BonusBearer.h
		mock/TownFake.h
		mock/TinyH3MWriter.h
		mock/TinyH3MBuilder.h

		quest/QuestTest.h
		quest/QuestAssertions.h
		quest/QuestScenarios.h
)

if(ENABLE_BATTLE_AI)
	file(GLOB_RECURSE BATTLE_AI_TEST_SRCS "../AI/BattleAI/*.cpp")
	list(FILTER BATTLE_AI_TEST_SRCS EXCLUDE REGEX ".*main\\.cpp$")

	list(APPEND test_SRCS
		${BATTLE_AI_TEST_SRCS}
		battleAI/SpellTargetsEvaluatorTest.cpp
	)
endif()

if(ENABLE_NULLKILLER2_AI)
	list(APPEND test_SRCS
		nullkiller2/AIUtilityTest.cpp
		nullkiller2/Analyzers/HeroManagerTest.cpp
		nullkiller2/Analyzers/ObjectClusterizerTest.cpp
		nullkiller2/Behaviors/DefenceBehaviorTest.cpp
		nullkiller2/Behaviors/EscapeBehaviorTest.cpp
		nullkiller2/Behaviors/ExplorationBehaviorTest.cpp
		nullkiller2/Behaviors/RecruitHeroBehaviorTest.cpp
		nullkiller2/Engine/PriorityEvaluatorTest.cpp
		nullkiller2/Engine/TaskFailureTest.cpp
		nullkiller2/Goals/BuyArmyTest.cpp
		nullkiller2/Goals/CompositionTest.cpp
		nullkiller2/Goals/ExploreNeighbourTileTest.cpp
		nullkiller2/Helpers/ExplorationHelperTest.cpp
		nullkiller2/Helpers/TinyH3MDimensionDoorExplorationTest.cpp
		nullkiller2/Pathfinding/ArmyLossTest.cpp
		nullkiller2/Pathfinding/DimensionDoorActionTest.cpp
	)

	list(APPEND test_HEADERS
		nullkiller2/Nulkiller2TestUtils.h
	)
endif()

assign_source_group(${test_SRCS} ${test_HEADERS})

set(mock_HEADERS
		mock/mock_battle_IBattleState.h
		mock/mock_battle_Unit.h
		mock/mock_Creature.h
		mock/mock_CreatureService.h
		mock/mock_IBattleInfoCallback.h
		mock/mock_scripting_Pool.h
		mock/mock_spells_Mechanics.h
		mock/mock_spells_Problem.h
		mock/mock_spells_Spell.h
		mock/mock_UnitEnvironment.h
		mock/mock_UnitInfo.h
		mock/mock_vstd_RNG.h
		mock/mock_CPSICallback.h
)

if(MSVC)
	set(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) run-time lib even when Google Test is built as static lib." FORCE)
endif()
check_cxx_compiler_flag(-Wcharacter-conversion CHARCONV)
if(CHARCONV)
	add_compile_options(-Wno-error=character-conversion) #Used in googletest
endif()
check_cxx_compiler_flag(-Wimplicit-int-float-conversion CONV)
if(CONV)
	add_compile_options(-Wno-error=implicit-int-float-conversion) #Used in googletest
endif()
check_cxx_compiler_flag(-Wdeprecated-copy-with-user-provided-copy COPY)
if(COPY)
	add_compile_options(-Wno-deprecated-copy-with-user-provided-copy) #Used in googletest
endif()
check_cxx_compiler_flag(-Wvirtual-move-assign MOVE_ASSIGN)
if(MOVE_ASSIGN)
	add_compile_options(-Wno-error=virtual-move-assign) #GCC is too strict here
endif()
add_subdirectory_with_folder("3rdparty" googletest EXCLUDE_FROM_ALL)

add_executable(vcmitest ${test_SRCS} ${test_HEADERS} ${mock_HEADERS})
if(ENABLE_NULLKILLER2_AI)
	# Nullkiller tests use internal AI symbols, so embed AI objects in vcmitest
	# instead of linking the facade and loading a second copy of lib globals.
	target_sources(vcmitest PRIVATE $<TARGET_OBJECTS:Nullkiller2>)
	set(vcmitest_LIBS gtest gmock vcmiMain vcmiservercommon TBB::tbb ${SYSTEM_LIBS})
	foreach(optional_vcmitest_target IN ITEMS vcmiLua EmptyAI StupidAI MMAI)
		if(TARGET ${optional_vcmitest_target})
			list(APPEND vcmitest_LIBS ${optional_vcmitest_target})
		endif()
	endforeach()
else()
	set(vcmitest_LIBS gtest gmock vcmi vcmiservercommon ${SYSTEM_LIBS})
endif()
target_link_libraries(vcmitest PRIVATE ${vcmitest_LIBS})

target_include_directories(vcmitest
		PUBLIC	${CMAKE_CURRENT_SOURCE_DIR}
		PRIVATE	${GTestSrc}
		PRIVATE	${GTestSrc}/include
		PRIVATE	${GMockSrc}
		PRIVATE	${GMockSrc}/include
)

# Running tests one by one using ctest not recommended due to vcmi having
# slow global initialization.
gtest_discover_tests(vcmitest
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

vcmi_set_output_dir(vcmitest "")

enable_pch(vcmitest)

# This isolated executable intentionally does not link client globals. It proves
# CExchangeWindow presentation state without duplicating game-rule callbacks.
add_executable(vcmi_exchange_panel_navigation_spike_test
	client/CExchangePanelNavigationSpikeTest.cpp
	../client/widgets/CExchangePanelNavigationSpike.cpp
	../client/widgets/CExchangePanelNavigationSpike.h
	../client/windows/CExchangeWindowPanelRequestAdapter.cpp
	../client/windows/CExchangeWindowPanelRequestAdapter.h
)
target_compile_features(vcmi_exchange_panel_navigation_spike_test PRIVATE cxx_std_20)
target_include_directories(vcmi_exchange_panel_navigation_spike_test PRIVATE ${CMAKE_SOURCE_DIR})
add_test(
	NAME EXCHANGE-PANEL-NAVIGATION-SPIKE
	COMMAND vcmi_exchange_panel_navigation_spike_test
)
set_tests_properties(
	EXCHANGE-PANEL-NAVIGATION-SPIKE
	PROPERTIES
		LABELS "controller;complex-ui;exchange;navigation;pure-spike"
)

# This target remains outside vcmitest because it must prove a pure startup
# resolver without constructing GameLibrary or VCMIDirs.
if(APPLE AND NOT IOS)
	add_executable(vcmi_userroot_resolver_test UserRootResolverTest.cpp)
	target_link_libraries(vcmi_userroot_resolver_test PRIVATE gtest)
	add_test(NAME USERROOT-RESOLVER-UNIT COMMAND vcmi_userroot_resolver_test)
	set_tests_properties(USERROOT-RESOLVER-UNIT PROPERTIES LABELS "macos;userroot;pure-resolver")

	add_executable(vcmi_userroot_runtime_probe UserRootRuntimeProbe.cpp)
	target_link_libraries(vcmi_userroot_runtime_probe PRIVATE vcmi)

	add_executable(vcmi_userroot_runtime_test UserRootRuntimeTest.cpp)
	target_link_libraries(vcmi_userroot_runtime_test PRIVATE gtest)
	add_dependencies(vcmi_userroot_runtime_test vcmi_userroot_runtime_probe)
	add_test(
		NAME USERROOT-RUNTIME-OVERRIDE
		COMMAND vcmi_userroot_runtime_test $<TARGET_FILE:vcmi_userroot_runtime_probe>
	)
	set_tests_properties(USERROOT-RUNTIME-OVERRIDE PROPERTIES LABELS "macos;userroot;fresh-process;runtime")

	# This test needs a fresh process because SettingsStorage and VCMIDirs are
	# singletons. The parent supplies a new VCMI_USER_ROOT for every probe and
	# the probe initializes the production settings loader against the real schema.
	add_executable(vcmi_controller_trigger_threshold_contract_probe ControllerTriggerThresholdContractProbe.cpp)
	target_link_libraries(vcmi_controller_trigger_threshold_contract_probe PRIVATE vcmi)

	add_executable(vcmi_controller_trigger_threshold_contract_test ControllerTriggerThresholdContractTest.cpp)
	target_link_libraries(vcmi_controller_trigger_threshold_contract_test PRIVATE gtest)
	add_dependencies(vcmi_controller_trigger_threshold_contract_test vcmi_controller_trigger_threshold_contract_probe)
	add_test(
		NAME CONTROLLER-TRIGGER-THRESHOLD-CONTRACT
		COMMAND vcmi_controller_trigger_threshold_contract_test $<TARGET_FILE:vcmi_controller_trigger_threshold_contract_probe>
	)
	set_tests_properties(
		CONTROLLER-TRIGGER-THRESHOLD-CONTRACT
		PROPERTIES
			LABELS "macos;controller;settings;fresh-process;contract"
			WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
	)
endif()

file (GLOB_RECURSE testdata "testdata/*.*")
foreach(resource ${testdata})
	get_filename_component(filename ${resource} NAME)
	get_filename_component(dir ${resource} DIRECTORY)
	get_filename_component(dirname ${dir} NAME)
	set (output "")
	while(NOT ${dirname} STREQUAL testdata)
		get_filename_component(path_component ${dir} NAME)
		set (output "${path_component}/${output}")
		get_filename_component(dir ${dir} DIRECTORY)
		get_filename_component(dirname ${dir} NAME)
	endwhile()
	set(output "${CMAKE_BINARY_DIR}/bin/test/testdata/${output}/${filename}")
	configure_file(${resource} ${output} COPYONLY)
endforeach()
