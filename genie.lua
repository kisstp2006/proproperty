if plugin "proproperty" then
	files { 
		"src/**.c",
		"src/**.cpp",
		"src/**.h",
		"genie.lua"
	}
	defines { "BUILDING_PROPROPERTY" }
	dynamic_link_plugin { "engine", "core" }
	if build_studio then
		dynamic_link_plugin { "editor" }
	end

end