cmake_minimum_required (VERSION 2.8.11)
project (rai)

set (CPACK_PACKAGE_VERSION_MAJOR "14")
set (CPACK_PACKAGE_VERSION_MINOR "0")
set (CPACK_PACKAGE_VERSION_PATCH "0")
if (DEFINED GIT_COMMIT)
set (CPACK_PACKAGE_VERSION_PATCH "GIT-${GIT_COMMIT}")
endif (DEFINED GIT_COMMIT)
set (CPACK_PACKAGE_VENDOR "Badem Currency")

set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")

set (BADEM_GUI OFF CACHE BOOL "")
set (BADEM_TEST OFF CACHE BOOL "")
set (BADEM_SECURE_RPC OFF CACHE BOOL "")

option(BADEM_ASAN_INT "Enable ASan+UBSan+Integer overflow" OFF)
option(BADEM_ASAN "Enable ASan+UBSan" OFF)
option(BADEM_SIMD_OPTIMIZATIONS "Enable CPU-specific SIMD optimizations (SSE/AVX or NEON, e.g.)" OFF)
set (BOOST_CUSTOM OFF CACHE BOOL "")

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if (WIN32)
	set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN -DMINIUPNP_STATICLIB")
	set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
else (WIN32)
	set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -Werror=switch -fPIC")
	if (BADEM_ASAN)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined -DED25519_NO_INLINE_ASM")
	elseif (BADEM_ASAN_INT)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined,integer -DED25519_NO_INLINE_ASM")
	elseif (BADEM_TSAN)
		set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=thread -DED25519_NO_INLINE_ASM")
	endif()

	IF (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$")
		if (BADEM_SIMD_OPTIMIZATIONS)
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse4")
		elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse2")
		endif()

		set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
		if (ENABLE_AVX2)
			set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -mavx2 -mbmi -mbmi2")
			if (PERMUTE_WITH_GATHER)
				set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_GATHER")
			elseif (PERMUTE_WITH_SHUFFLES)
				set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_SHUFFLES")
			endif()
		endif()
	else()
		set (BLAKE2_IMPLEMENTATION "blake2/blake2b-ref.c")
	endif()
endif (WIN32)

if (WIN32)
	set (PLATFORM_C_FLAGS "/std=c11")
else (WIN32)
	set (PLATFORM_C_FLAGS "-std=c11")
endif (WIN32)

if (WIN32)
	set (PLATFORM_CXX_FLAGS "/bigobj")
else (WIN32)
	set (PLATFORM_CXX_FLAGS "-std=c++14")
endif (WIN32)

if (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
else (WIN32)
	set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif (WIN32)

if (APPLE)
	set (PLATFORM_LINK_FLAGS "-framework Foundation -framework OpenCL")
elseif (WIN32)
	set (PLATFORM_LINK_FLAGS "")
else ()
	set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
	if (BADEM_ASAN)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined")
	elseif (BADEM_ASAN_INT)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined,integer")
	elseif (BADEM_TSAN)
		set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=thread")
	endif()
endif (APPLE)

if (BADEM_GUI)
	if (WIN32)
		set (PLATFORM_QT_PACKAGES WinExtras)
	else (WIN32)
		set (PLATFORM_QT_PACKAGES)
	endif (WIN32)

	find_package (Qt5 COMPONENTS Core Gui Widgets Test ${PLATFORM_QT_PACKAGES})
	include_directories (${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Test_INCLUDE_DIRS})
endif (BADEM_GUI)

if (BADEM_SECURE_RPC)
	find_package (OpenSSL 1.0 EXACT REQUIRED)
	include_directories(${OPENSSL_INCLUDE_DIR})
	add_definitions (-DBADEM_SECURE_RPC)
	message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
	message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
	message("OpenSSL lib: ${OPENSSL_SSL_LIBRARY}")
	message("Crypto lib: ${OPENSSL_CRYPTO_LIBRARY}")
else ()
	set (OPENSSL_LIBRARIES "")
endif (BADEM_SECURE_RPC)

include_directories (${CMAKE_SOURCE_DIR})

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

if (BOOST_CUSTOM)
    list(APPEND CMAKE_MODULE_PATH ".\\" ${CMAKE_SOURCE_PATH})
endif (BOOST_CUSTOM)

find_package (Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem system log log_setup thread program_options regex chrono atomic)
include_directories (${Boost_INCLUDE_DIR})

add_library (ed25519
	ed25519-donna/ed25519-donna-portable.h
	ed25519-donna/ed25519-hash-custom.h
	ed25519-donna/ed25519-randombytes-custom.h
	ed25519-donna/ed25519.h
	ed25519-donna/ed25519.c)

include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/include")
include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/src")
include_directories("${CMAKE_SOURCE_DIR}/blake2")

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set (ARGON_CORE phc-winner-argon2/src/opt.c)
else ()
	set (ARGON_CORE phc-winner-argon2/src/ref.c)
endif ()

if (WIN32)
	set (gtest_force_shared_crt ON)
else ()
	set (gtest_force_shared_crt OFF)
endif()
add_subdirectory (gtest)
include_directories ("gtest/include")

include_directories (beast/include)

set (UPNPC_BUILD_SHARED OFF CACHE BOOL "")
add_subdirectory (miniupnp/miniupnpc)
include_directories (miniupnp/miniupnpc)

set (BUILD_SHARED OFF CACHE BOOL "")
set (BUILD_TESTING OFF CACHE BOOL "")
set (USE_INTERMEDIATE_OBJECTS_TARGET OFF CACHE BOOL "")
if (CRYPTOPP_CUSTOM OR NOT BADEM_SIMD_OPTIMIZATIONS)
	set (CRYPTOPP_LIBRARY cryptopp)
	add_library (cryptopp
	cryptopp/algparam.cpp
	cryptopp/asn.cpp
	cryptopp/basecode.cpp
	cryptopp/cpu.cpp
	cryptopp/cryptlib.cpp
	cryptopp/default.cpp
	cryptopp/des.cpp
	cryptopp/dessp.cpp
	cryptopp/dll.cpp
	cryptopp/ec2n.cpp
	cryptopp/ecp.cpp
	cryptopp/filters.cpp
	cryptopp/fips140.cpp
	cryptopp/gcm.cpp
	cryptopp/gf2n.cpp
	cryptopp/gfpcrypt.cpp
	cryptopp/hex.cpp
	cryptopp/hmac.cpp
	cryptopp/hrtimer.cpp
	cryptopp/integer.cpp
	cryptopp/iterhash.cpp
	cryptopp/misc.cpp
	cryptopp/modes.cpp
	cryptopp/mqueue.cpp
	cryptopp/nbtheory.cpp
	cryptopp/oaep.cpp
	cryptopp/osrng.cpp
	cryptopp/pubkey.cpp
	cryptopp/queue.cpp
	cryptopp/randpool.cpp
	cryptopp/rdtables.cpp
	cryptopp/rijndael.cpp
	cryptopp/rng.cpp
	cryptopp/sha.cpp
	cryptopp/simple.cpp
	cryptopp/winpipes.cpp)
	set_target_properties (cryptopp PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
else ()
	set (CRYPTOPP_LIBRARY cryptopp-static)
	add_subdirectory (cryptopp)
endif ()

add_library (argon2
	phc-winner-argon2/src/argon2.c
	phc-winner-argon2/include/argon2.h
	phc-winner-argon2/src/core.c
	phc-winner-argon2/src/thread.c
	phc-winner-argon2/src/encoding.c
	${ARGON_CORE})

add_library (xxhash
	xxhash/xxhash.c
	xxhash/xxhash.h)

add_library (lmdb
	lmdb/libraries/liblmdb/lmdb.h
	lmdb/libraries/liblmdb/mdb.c
	lmdb/libraries/liblmdb/midl.c)

add_library (blake2
	blake2/blake2-config.h
	blake2/blake2-impl.h
	blake2/blake2.h
	${BLAKE2_IMPLEMENTATION})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set (PLATFORM_LIB_SOURCE badem/plat/default/priority.cpp)
	set (PLATFORM_SECURE_SOURCE badem/plat/osx/working.mm)
	set (PLATFORM_WALLET_SOURCE badem/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set (PLATFORM_LIB_SOURCE badem/plat/windows/priority.cpp)
	set (PLATFORM_SECURE_SOURCE badem/plat/windows/working.cpp)
	set (PLATFORM_NODE_SOURCE badem/plat/windows/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE badem/plat/windows/icon.cpp Badem.rc)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set (PLATFORM_LIB_SOURCE badem/plat/linux/priority.cpp)
	set (PLATFORM_SECURE_SOURCE badem/plat/posix/working.cpp)
	set (PLATFORM_NODE_SOURCE badem/plat/posix/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE badem/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
	set (PLATFORM_LIB_SOURCE badem/plat/default/priority.cpp)
	set (PLATFORM_SECURE_SOURCE badem/plat/posix/working.cpp)
	set (PLATFORM_NODE_SOURCE badem/plat/posix/openclapi.cpp)
	set (PLATFORM_WALLET_SOURCE badem/plat/default/icon.cpp)
else ()
	error ("Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif ()

# Embed bootstrap representative weights in executable
file (READ rep_weights.bin filedata HEX)
string (REGEX REPLACE "(..)" "0x\\1," filedata ${filedata})
file (WRITE ${CMAKE_BINARY_DIR}/bootstrap_weights.cpp "#include <cstddef>\n"
	"namespace rai {\n"
	"	unsigned char badem_bootstrap_weights[] = {${filedata} 0x00};\n"
	"	size_t badem_bootstrap_weights_size = sizeof(badem_bootstrap_weights) - 1;\n"
	"}\n")
	
if (BADEM_SECURE_RPC)
	set (SECURE_RPC_SOURCE badem/node/rpc_secure.cpp badem/node/rpc_secure.hpp)
endif ()

add_library (secure
	${PLATFORM_SECURE_SOURCE}
	${CMAKE_BINARY_DIR}/bootstrap_weights.cpp
	badem/config.hpp
	badem/common.cpp
	badem/common.hpp
	badem/blockstore.cpp
	badem/blockstore.hpp
	badem/ledger.cpp
	badem/ledger.hpp
	badem/node/utility.cpp
	badem/node/utility.hpp
	badem/versioning.hpp
	badem/versioning.cpp)

SET (RAI_LIB_SOURCES
	${PLATFORM_LIB_SOURCE}
	badem/lib/blocks.cpp
	badem/lib/blocks.hpp
	badem/lib/interface.cpp
	badem/lib/interface.h
	badem/lib/numbers.cpp
	badem/lib/numbers.hpp
	badem/lib/utility.cpp
	badem/lib/utility.hpp
	badem/lib/work.hpp
	badem/lib/work.cpp)

add_library (badem_lib SHARED ${RAI_LIB_SOURCES})
add_library (badem_lib_static STATIC ${RAI_LIB_SOURCES})

add_library (node
	${PLATFORM_NODE_SOURCE}
	${SECURE_RPC_SOURCE}
	badem/node/bootstrap.cpp
	badem/node/bootstrap.hpp
	badem/node/common.cpp
	badem/node/common.hpp
	badem/node/node.hpp
	badem/node/node.cpp
	badem/node/openclwork.cpp
	badem/node/openclwork.hpp
	badem/node/rpc.hpp
	badem/node/rpc.cpp
	badem/node/testing.hpp
	badem/node/testing.cpp
	badem/node/wallet.hpp
	badem/node/wallet.cpp
	badem/node/stats.hpp
	badem/node/stats.cpp
	badem/node/working.hpp
	badem/node/xorshift.hpp)


SET (ACTIVE_NETWORK badem_live_network CACHE STRING "Selects which network parameters are used")
set_property (CACHE ACTIVE_NETWORK PROPERTY STRINGS badem_test_network badem_beta_network badem_live_network)

if (BADEM_TEST)
	add_executable (core_test
		badem/core_test/block.cpp
		badem/core_test/block_store.cpp
		badem/core_test/interface.cpp
		badem/core_test/conflicts.cpp
		badem/core_test/daemon.cpp
		badem/core_test/entry.cpp
		badem/core_test/gap_cache.cpp
		badem/core_test/ledger.cpp
		badem/core_test/network.cpp
		badem/core_test/node.cpp
		badem/core_test/message.cpp
		badem/core_test/message_parser.cpp
		badem/core_test/processor_service.cpp
		badem/core_test/peer_container.cpp
		badem/core_test/rpc.cpp
		badem/core_test/uint256_union.cpp
		badem/core_test/versioning.cpp
		badem/core_test/wallet.cpp
		badem/core_test/wallets.cpp
		badem/core_test/work_pool.cpp)

	add_executable (slow_test
		badem/slow_test/node.cpp)
	
	set_target_properties (core_test slow_test PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DBADEM_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DBADEM_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
	set_target_properties (core_test slow_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (BADEM_TEST)

if (BADEM_GUI)

	qt5_add_resources(RES resources.qrc)


	add_library (qt
		badem/qt/qt.cpp
		badem/qt/qt.hpp)

	add_executable (badem_wallet ${PLATFORM_GUI_TARGET_PROPERTIES}
		${PLATFORM_WALLET_SOURCE}
		badem/badem_wallet/entry.cpp
		${RES})

	add_executable (qt_test
		badem/qt_test/entry.cpp
		badem/qt_test/qt.cpp)

	add_executable (qt_system
		badem/qt_system/entry.cpp)

	set_target_properties (badem_wallet qt_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
	
	set_target_properties (qt_test qt badem_wallet qt_system PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DBADEM_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DBADEM_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
	set_target_properties (qt qt_system PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (BADEM_GUI)

add_executable (badem_node
	badem/badem_node/daemon.cpp
	badem/badem_node/daemon.hpp
	badem/badem_node/entry.cpp)

set_target_properties (argon2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
set_target_properties (blake2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -D__SSE2__")
set_target_properties (ed25519 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DED25519_CUSTOMHASH -DED25519_CUSTOMRNG")
set_target_properties (secure node badem_node badem_lib badem_lib_static PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DBADEM_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DBADEM_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
set_target_properties (secure node badem_node PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")

if (WIN32)
	set (PLATFORM_LIBS Ws2_32 mswsock iphlpapi ntdll)
else (WIN32)
	set (PLATFORM_LIBS pthread ${CMAKE_DL_LIBS})
endif (WIN32)

if (WIN32)
	set (PLATFORM_WALLET_LIBS Qt5::WinExtras)
else (WIN32)
	set (PLATFORM_WALLET_LIBS)
endif (WIN32)

if (BADEM_TEST)
	target_link_libraries (core_test node secure lmdb ed25519 badem_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (slow_test node secure lmdb ed25519 badem_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})
endif (BADEM_TEST)

if (BADEM_GUI)
	target_link_libraries (qt_test node secure lmdb ed25519 badem_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Test ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (qt_system node secure lmdb ed25519 badem_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

	target_link_libraries (badem_wallet node secure lmdb ed25519 badem_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS} ${PLATFORM_WALLET_LIBS})
endif (BADEM_GUI)

target_link_libraries (badem_lib ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})
target_link_libraries (badem_lib_static ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})

target_link_libraries (badem_node node secure lmdb ed25519 badem_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

set (CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
if (BADEM_GUI)
	if (APPLE)
		install (TARGETS banano_wallet DESTINATION Banano.app/Contents/MacOS)
		install (FILES Badem.icns DESTINATION Badem.app/Contents/Resources)
	elseif (WIN32)
		if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
			set (WIN_REDIST vc_redist.x64.exe)
		else ()
			set (WIN_REDIST vc_redist.x86.exe)
		endif ()
		set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\"$INSTDIR\\\\${WIN_REDIST}\\\" /quiet /norestart'")
		set (CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/Badem.ico)
		set (CPACK_NSIS_DISPLAY_NAME "Badem")
		set (CPACK_PACKAGE_NAME "Badem_Installer")
		set (CPACK_NSIS_PACKAGE_NAME "Badem ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
		set (CPACK_NSIS_URL_INFO_ABOUT "https://website.org")
		set (CPACK_NSIS_CONTACT "contact@website.org")
		set (CPACK_NSIS_MENU_LINKS "badem_wallet.exe" "Badem Wallet" "https://website.org" "Badem website")
		set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
		get_target_property (Qt5WindowsPlugin Qt5::QWindowsIntegrationPlugin LOCATION)
		get_filename_component (Qt5_bin_DIR ${Qt5_DIR}/../../../bin ABSOLUTE)
		install (TARGETS badem_wallet DESTINATION .)
		install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${WIN_REDIST} DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/libGLESv2.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Core.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Gui.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5Widgets.dll DESTINATION .)
		install (FILES ${Qt5_bin_DIR}/Qt5WinExtras.dll DESTINATION .)
		install (FILES ${Qt5WindowsPlugin} DESTINATION platforms)
	else ()
		install(TARGETS badem_wallet badem_node badem_lib
				RUNTIME DESTINATION bin
				LIBRARY DESTINATION lib
				ARCHIVE DESTINATION lib
		)
	endif ()
endif (BADEM_GUI)

include (CPack)