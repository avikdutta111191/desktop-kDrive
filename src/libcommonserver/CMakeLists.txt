project(libcommonserver)

find_package(SQLite3 3.8.0 REQUIRED)
find_package(Poco 1.12.5 REQUIRED Foundation Net JSON Util)
find_package(OpenSSL 3.1.0 REQUIRED SSL Crypto)
find_package(log4cplus 2.1.0 REQUIRED)
find_package(xxHash 0.8.2 REQUIRED)
find_package(libzip 1.10.1 REQUIRED)

add_definitions(-DUNICODE)
add_definitions(-D_UNICODE)

# Sources
set(libcommonserver_SRCS
    # Utility
    utility/utility.h utility/utility.cpp
    utility/asserts.h
    utility/stateholder.h
    # Db
    db/sqlitedb.h db/sqlitedb.cpp
    db/sqlitequery.h db/sqlitequery.cpp
    db/db.h db/db.cpp
    db/dbdefs.h
    # Log
    log/log.h log/log.cpp
    log/customrollingfileappender.h log/customrollingfileappender.cpp
    # Network
    network/proxy.h network/proxy.cpp
    network/proxyconfig.h network/proxyconfig.cpp
    # Io
    io/filestat.h
    io/iohelper.h io/iohelper.cpp
)

if(APPLE)
    list(APPEND libcommonserver_SRCS io/iohelper_mac.mm io/iohelper_mac.cpp)
    list(APPEND libcommonserver_SRCS utility/utility_mac.mm)
elseif(WIN32)
    list(APPEND libcommonserver_SRCS io/iohelper_win.h io/iohelper_win.cpp)
else()
    list(APPEND libcommonserver_SRCS io/iohelper_linux.cpp)
endif()

if (USE_OUR_OWN_SQLITE3)
    list(APPEND libcommonserver_SRCS ${SQLITE3_SOURCE})
endif()

# Target
add_library(${libcommonserver_NAME} STATIC ${libcommonserver_SRCS})

set_target_properties(${libcommonserver_NAME} PROPERTIES
    VERSION ${KDRIVE_VERSION}
    SOVERSION ${KDRIVE_SOVERSION}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIRECTORY}
)

GENERATE_EXPORT_HEADER(${libcommonserver_NAME}
    BASE_NAME ${libcommonserver_NAME}
    EXPORT_MACRO_NAME COMMONSERVER_EXPORT
    EXPORT_FILE_NAME commonserverlib.h
)

# Includes
if (WIN32)
    target_include_directories(${libcommonserver_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "C:/Program Files (x86)/log4cplus/include"
        "C:/Program Files (x86)/libzip/include"
    )
else()
    target_include_directories(${libcommonserver_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        "/usr/local/include"
        ${CMAKE_SOURCE_DIR}/src/3rdparty/utf8proc)
endif()

if (USE_OUR_OWN_SQLITE3)
    # make sure that the path for the local sqlite3 is before the system one
    target_include_directories("${libcommonserver_NAME}" BEFORE PRIVATE ${SQLITE3_INCLUDE_DIR})
else()
    target_include_directories("${libcommonserver_NAME}" PRIVATE ${SQLITE3_INCLUDE_DIR})
endif()

# Link
target_link_libraries(${libcommonserver_NAME}
    ${libcommon_NAME}
    ${SQLITE3_LIBRARIES}
    Poco::Foundation Poco::Net Poco::JSON Poco::Util
    OpenSSL::SSL OpenSSL::Crypto
    libzip::zip)

if (WIN32)
    target_link_libraries(${libcommonserver_NAME}
        log4cplus::log4cplusU
        xxHash::xxhash
        Secur32)
elseif(APPLE)
    # utility_mac.mm
    find_library(FOUNDATION_LIBRARY NAMES Foundation)
    find_library(CORESERVICES_LIBRARY NAMES CoreServices)
    find_library(APPKIT_LIBRARY NAMES AppKit)

    target_link_libraries(${libcommonserver_NAME}
        "/usr/local/lib/liblog4cplusU.dylib"
        "/usr/local/lib/libxxhash.0.dylib"
        ${FOUNDATION_LIBRARY}
        ${CORESERVICES_LIBRARY}
        ${APPKIT_LIBRARY}
        utf8proc)
else()
    target_link_libraries(${libcommonserver_NAME}
        "/usr/local/lib/liblog4cplusU.so"
        "/usr/local/lib/libxxhash.so"
        utf8proc)
endif()

