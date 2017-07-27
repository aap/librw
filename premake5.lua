workspace "librw"
	location "build"
	language "C++"

	configurations { "Release", "Debug" }
	filter { "system:windows" }
		configurations { "ReleaseStatic" }
		platforms { "win-x86-null", "win-x86-gl3", "win-x86-d3d9",
			"win-amd64-null", "win-amd64-gl3", "win-amd64-d3d9" }
	filter { "system:linux" }
		platforms { "linux-x86-null", "linux-x86-gl3",
		"linux-amd64-null", "linux-amd64-gl3" }
	-- TODO: ps2
	filter {}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
	filter "configurations:Release*"
		defines { "NDEBUG" }
		optimize "On"
	filter "configurations:ReleaseStatic"
		flags { "StaticRuntime" }

	filter { "platforms:*null" }
		defines { "RW_NULL" }
	filter { "platforms:*gl3" }
		defines { "RW_GL3" }
	filter { "platforms:*d3d9" }
		defines { "RW_D3D9" }
	filter { "platforms:*ps2" }
		defines { "RW_PS2" }

	filter { "platforms:*amd64*" }
		architecture "x86_64"
	filter { "platforms:*x86*" }
		architecture "x86"

	filter { "platforms:win*" }
		system "windows"
	filter { "platforms:linux*" }
		system "linux"

	filter "action:vs*"
		buildoptions { "/wd4996" }

	filter {}

	Libdir = "lib/%{cfg.platform}/%{cfg.buildcfg}"
	Bindir = "bin/%{cfg.platform}/%{cfg.buildcfg}"

project "librw"
	kind "StaticLib"
	targetname "rw"
	targetdir (Libdir)
	files { "src/*.*" }
	files { "src/*/*.*" }

project "dumprwtree"
	kind "ConsoleApp"
	targetdir (Bindir)
	files { "tools/dumprwtree/*" }
	includedirs { "." }
	libdirs { Libdir }
	links { "librw" }
