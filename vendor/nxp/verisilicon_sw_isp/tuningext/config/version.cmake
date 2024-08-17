message("Load Version...")

string(TIMESTAMP NOW "%Y-%m-%d %H:%M:%S")

set(VERSION_CAMERA "4.3.0")
set(VERSION_SHELL "4.3.0")

add_definitions(-DBUILD_TIME="${NOW}" -DVERSION_CAMERA="${VERSION_CAMERA}"
                -DVERSION_SHELL="${VERSION_SHELL}")
