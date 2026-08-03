add_executable(vcmi_launcher_controller_spike_test
	LauncherControllerSpike.cpp
	LauncherControllerSpike.h
	QtLauncherControllerSpikeTest.cpp
)

if(TARGET Qt6::Widgets)
	target_link_libraries(vcmi_launcher_controller_spike_test PRIVATE Qt6::Widgets)
elseif(TARGET Qt5::Widgets)
	target_link_libraries(vcmi_launcher_controller_spike_test PRIVATE Qt5::Widgets)
else()
	message(FATAL_ERROR "Qt Widgets target is required for the launcher controller spike")
endif()

target_compile_definitions(vcmi_launcher_controller_spike_test
	PRIVATE
		VCMI_QT_LAUNCHER_SPIKE_FIXTURE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/qt-controller-events.json"
)

add_test(
	NAME QT-LAUNCHER-CONTROLLER-SPIKE
	COMMAND vcmi_launcher_controller_spike_test
)

set_tests_properties(
	QT-LAUNCHER-CONTROLLER-SPIKE
	PROPERTIES
		ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
		LABELS "launcher;controller;qt;spike"
)
