newoption {
	trigger		= "gfxlib",
	value       = "LIBRARY",
	description = "Choose a particular development library",
	default		= "glfw",
	allowed		= {
		{ "glfw",	"GLFW" },
		{ "sdl2",	"SDL2" },
		{ "sdl3",	"SDL3" },
	},
}

newoption {
	trigger     = "glfwdir64",
	value       = "PATH",
	description = "Directory of glfw",
	default     = "../glfw-3.3.4.bin.WIN64",
}

newoption {
	trigger     = "glfwdir32",
	value       = "PATH",
	description = "Directory of glfw",
	default     = "../glfw-3.3.4.bin.WIN32",
}

newoption {
	trigger     = "sdl2dir",
	value       = "PATH",
	description = "Directory of sdl2",
	default     = "../SDL2-2.32.10",
}

newoption {
	trigger     = "sdl3dir",
	value       = "PATH",
	description = "Directory of sdl3",
	default     = "../SDL3-3.2.22",
}

local d3d11Shaders = {
	"clear_VS_d11",
	"clear_PS_d11",
	"im2d_VS_d11",
	"im2d_PS_d11",
	"im3d_VS_d11",
	"im3d_PS_d11",
	"im3d_tex_PS_d11",
	"default_VS_d11",
	"default_PS_d11",
	"skin_VS_d11",
	"matfx_env_VS_d11",
	"matfx_env_PS_d11",
}

local d3d11GeneratedDir = path.join(_SCRIPT_DIR, "build/generated")

local function generateD3D11ShaderHeaders()
	os.mkdir(d3d11GeneratedDir)
	for _, shaderName in ipairs(d3d11Shaders) do
		local input = assert(io.open(path.join(
			_SCRIPT_DIR, "src/d3d/shaders", shaderName .. ".hlsl"), "rb"))
		local source = input:read("*all")
		input:close()

		local output = assert(io.open(path.join(
			d3d11GeneratedDir, shaderName .. ".h"), "wb"))
		output:write("static const char ", shaderName,
			"_source[] = R\"librw_shader(\n", source,
			"\n)librw_shader\";\n")
		output:close()
	end
end

generateD3D11ShaderHeaders()

workspace "librw"
	location "build"
	language "C++"

	configurations { "Release", "Debug" }
	filter { "system:windows" }
		configurations { "ReleaseStatic" }
		platforms { "win-x86-null", "win-x86-gl3", "win-x86-d3d9", "win-x86-d3d11",
			"win-amd64-null", "win-amd64-gl3", "win-amd64-d3d9", "win-amd64-d3d11" }
	filter { "system:linux" }
		platforms { "linux-x86-null", "linux-x86-gl3",
		"linux-amd64-null", "linux-amd64-gl3",
		"linux-arm-null", "linux-arm-gl3",
		"ps2" }
		if _OPTIONS["gfxlib"] == "sdl2" then
			includedirs { "/usr/include/SDL2" }
		end
	filter {}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
	filter "configurations:Release*"
		defines { "NDEBUG" }
		optimize "On"
--	filter "configurations:ReleaseStatic"
--		staticruntime("On")

	filter { "platforms:*null" }
		defines { "RW_NULL" }
	filter { "platforms:*gl3" }
		defines { "RW_GL3" }
		if _OPTIONS["gfxlib"] == "sdl2" then
			defines { "LIBRW_SDL2" }
		elseif _OPTIONS["gfxlib"] == "sdl3" then
			defines { "LIBRW_SDL3" }
		elseif _OPTIONS["gfxlib"] == "glfw" then
			defines { "LIBRW_GLFW" }
		end
	filter { "platforms:*d3d9" }
		defines { "RW_D3D9" }
	filter { "platforms:*d3d11" }
		defines { "RW_D3D11" }
	filter { "platforms:ps2" }
		defines { "RW_PS2" }
		toolset "gcc"
		gccprefix 'ee-'
		buildoptions { "-nostdlib", "-fno-common" }
		includedirs { "$(PS2SDK)/ee/include", "$(PS2SDK)/common/include" }
		optimize "Off"

	filter { "platforms:*amd64*" }
		architecture "x86_64"
	filter { "platforms:*x86*" }
		architecture "x86"
	filter { "platforms:*arm*" }
		architecture "ARM"

	filter { "platforms:win*" }
		system "windows"
	filter { "platforms:linux*" }
		system "linux"

	filter { "platforms:win*gl3" }
		includedirs { path.join(_OPTIONS["sdl2dir"], "include") }
	filter { "platforms:win-x86-gl3" }
		includedirs { path.join(_OPTIONS["glfwdir32"], "include") }
	filter { "platforms:win-amd64-gl3" }
		includedirs { path.join(_OPTIONS["glfwdir64"], "include") }

	filter "action:vs*"
		buildoptions { "/wd4996", "/wd4244" }

	filter { "platforms:win*gl3", "action:not vs*" }
		if _OPTIONS["gfxlib"] == "sdl2" then
			includedirs { "/mingw/include/SDL2" } -- TODO: Detect this properly
		end

	filter {}

	Libdir = "lib/%{cfg.platform}/%{cfg.buildcfg}"
	Bindir = "bin/%{cfg.platform}/%{cfg.buildcfg}"

function vucode()
	filter "files:**.dsm"
		buildmessage 'dvp-as %{file.name}'
		buildcommands {
			'cpp -x assembler-with-cpp "%{file.abspath}" | ee-dvp-as -I "%{file.directory}" -o "%{cfg.objdir}/%{file.basename}.o"'
		}
		buildoutputs { '%{cfg.objdir}/%{file.basename}.o' }
	filter {}
end

project "librw"
	kind "StaticLib"
	targetname "rw"
	targetdir (Libdir)
	defines { "LODEPNG_NO_COMPILE_CPP" }
	files { "src/*.*" }
	files { "src/*/*.*" }
	filter { "platforms:*gl3" }
		files { "src/gl/glad/*.*" }
<<<<<<< master
	filter { "platforms:*d3d11" }
		includedirs { d3d11GeneratedDir }
		files { "src/d3d/shaders/*_d11.hlsl" }
	filter { "files:**.hlsl" }
		buildaction "None"
	filter {}
=======
        vucode()
        filter { "platforms:ps2" }
                files { "src/ps2/vu1/*.dsm" }

>>>>>>> master

project "dumprwtree"
	kind "ConsoleApp"
	targetdir (Bindir)
	removeplatforms { "*gl3", "*d3d9", "*d3d11", "ps2" }
	files { "tools/dumprwtree/*" }
	includedirs { "." }
	libdirs { Libdir }
	links { "librw" }

function findlibs()
	filter { "platforms:linux*gl3" }
		links { "GL" }
		if _OPTIONS["gfxlib"] == "glfw" then
			links { "glfw" }
		elseif _OPTIONS["gfxlib"] == "sdl2" then
			links { "SDL2" }
		elseif _OPTIONS["gfxlib"] == "sdl3" then
			links { "SDL3" }
		end
	filter { "platforms:win-amd64-gl3" }
		libdirs { path.join(_OPTIONS["glfwdir64"], "lib-vc2015") }
		libdirs { path.join(_OPTIONS["sdl2dir"], "lib/x64") }
		libdirs { path.join(_OPTIONS["sdl3dir"], "lib/x64") }
	filter { "platforms:win-x86-gl3" }
		libdirs { path.join(_OPTIONS["glfwdir32"], "lib-vc2015") }
		libdirs { path.join(_OPTIONS["sdl2dir"], "lib/x86") }
		libdirs { path.join(_OPTIONS["sdl3dir"], "lib/x86") }
	filter { "platforms:win*gl3" }
		links { "opengl32" }
		if _OPTIONS["gfxlib"] == "glfw" then
			links { "glfw3" }
		elseif _OPTIONS["gfxlib"] == "sdl2" then
			links { "SDL2" }
		elseif _OPTIONS["gfxlib"] == "sdl3" then
			links { "SDL3" }
		end
	filter { "platforms:*d3d9" }
		links { "gdi32", "d3d9" }
	filter { "platforms:*d3d9", "action:vs*" }
		links { "Xinput9_1_0" }
	filter { "platforms:*d3d11" }
		links { "gdi32", "d3d11", "dxgi", "d3dcompiler" }
	filter { "platforms:*d3d11", "action:vs*" }
		links { "Xinput9_1_0" }
	filter {}
end

function skeleton()
	files { "skeleton/*.cpp", "skeleton/*.h" }
	files { "skeleton/imgui/*.cpp", "skeleton/imgui/*.h" }
	includedirs { "skeleton" }
end

function skeltool(dir)
	targetdir (Bindir)
	files { path.join("tools", dir, "*.cpp"),
	        path.join("tools", dir, "*.h") }
	vpaths {
		{["src"] = { path.join("tools", dir, "*") }},
		{["skeleton"] = { "skeleton/*" }},
	}
	skeleton()
	debugdir ( path.join("tools", dir) )
	includedirs { "." }
	libdirs { Libdir }
	links { "librw" }
	findlibs()
end

project "playground"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("playground")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" } -- for now

project "imguitest"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("imguitest")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "lights"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("lights")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "subrast"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("subrast")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "camera"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("camera")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "im2d"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("im2d")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "im3d"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("im3d")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

project "demoreel"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("demoreel")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" } -- for now

project "ska2anm"
	kind "ConsoleApp"
	characterset ("MBCS")
	targetdir (Bindir)
	files { path.join("tools/ska2anm", "*.cpp"),
	        path.join("tools/ska2anm", "*.h") }
	debugdir ( path.join("tools/ska2nm") )
	includedirs { "." }
	libdirs { Libdir }
	links { "librw" }
	findlibs()
	removeplatforms { "*gl3", "*d3d9", "*d3d11", "*ps2" }

--project "ps2test"
--	kind "ConsoleApp"
--	targetdir (Bindir)
--	vucode()
--	removeplatforms { "*gl3", "*d3d9", "*null" }
--	targetextension '.elf'
--	includedirs { "." }
--	files { "tools/ps2test/*.cpp",
--	        "tools/ps2test/vu/*.dsm",
--	        "tools/ps2test/*.h" }
--	libdirs { "$(PS2SDK)/ee/lib" }
--	links { "librw" }

--project "ps2rastertest"
--	kind "ConsoleApp"
--	targetdir (Bindir)
--	removeplatforms { "*gl3", "*d3d9" }
--	files { "tools/ps2rastertest/*.cpp" }
--	includedirs { "." }
--	libdirs { Libdir }
--	links { "librw" }

project "hopalong"
	kind "WindowedApp"
	characterset ("MBCS")
	skeltool("hopalong")
	entrypoint("WinMainCRTStartup")
	removeplatforms { "*null" }
	removeplatforms { "ps2" }

