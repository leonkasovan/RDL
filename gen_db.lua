--
-- Lua Script for generating Roms Search Database(csv) from website (archive.org)
-- Run: luaxx gen_db.lua
-- mv *.csv db/

------------------------------------------------------------------------------------------------------------
-- STRICT LUA SYNTAX
------------------------------------------------------------------------------------------------------------
local getinfo, error, rawset, rawget = debug.getinfo, error, rawset, rawget
local DB_PATH = "db"

local mt = getmetatable(_G)
if mt == nil then
  mt = {}
  setmetatable(_G, mt)
end

mt.__declared = {}

local function what ()
  local d = getinfo(3, "S")
  return d and d.what or "C"
end

mt.__newindex = function (t, n, v)
  if not mt.__declared[n] then
    local w = what()
    if w ~= "main" and w ~= "C" then
      error("assign to undeclared variable '"..n.."'", 2)
    end
    mt.__declared[n] = true
  end
  rawset(t, n, v)
end

mt.__index = function (t, n)
  if not mt.__declared[n] and what() ~= "C" then
    error("variable '"..n.."' is not declared", 2)
  end
  return rawget(t, n)
end
------------------------------------------------------------------------------------------------------------
local content_format_zipped = '<tr><td><a href=".-%.zip/(.-)">.-/*(.-)</a><td><td>.-<td id="size">(.-)</tr>'
local content_format_folder = '<td><a href="(.-)">(.-)</a>.-</td>.-<td>.-</td>.-<td>(.-)</td>.-</tr>'

-- str1 = "12345" return true
-- str2 = "abc123" return false
function is_string_all_digit(str)
    return string.match(str, "%D") == nil and string.len(str) > 0
end

function format_number(s)
    local pos = string.len(s) % 3

    if pos == 0 then pos = 3 end
    return string.sub(s, 1, pos).. string.gsub(string.sub(s, pos+1), "(...)", ".%1")
end

function format_bytes(s)
    local units = {"B", "KB", "MB", "GB", "TB", "PB", "EB"}
    local unitIndex = 1
    local size
	
	if type(s) == "string" then
		size = tonumber(s)
	elseif type(s) == "number" then
		size = s
	else
		return false
	end
    while size >= 1024 and unitIndex < #units do
        size = size / 1024
        unitIndex = unitIndex + 1
    end

    return string.format("%.2f %s", size, units[unitIndex])
end

-- url=https://archive.org/download/cylums-snes-rom-collection/Gamelist.txt
-- url=https://archive.org/download/cylums-final-burn-neo-rom-collection/Gamelist.txt
-- return table of game list
function cylum_load_gamelist(url)
	local game_list, no, format_gamelist
	
	print("Downloading Gamelist.txt ...")
	local rc, headers, content = http.request(url)
	if rc ~= 0 then
		print("Error: "..http.error(rc), rc)
		return nil
	end
	
	print("Importing Gamelist.txt ...")
	game_list = {}
	no = 0
	format_gamelist = nil
	for line in content:gmatch("[^\r\n]+") do
		local game_id, game_year, game_publisher, game_genre, game_player, game_title
		if #line > 60 then
			if format_gamelist == nil then	-- guess gamelist format: 5 coloumn or 6 coloumn
				game_id, game_year, game_publisher, game_genre, game_player, game_title = line:match("(.-) %-* (%w-) %-%- (.-) %-%-%-* (.-) %-%-%-* (.-) %-%-%-* (.-)$")
				if game_title ~= nil then
					format_gamelist = 6
				else
					game_year, game_publisher, game_genre, game_player, game_title = line:match("(.-) %-%- (.-) %-%-%-* (.-) %-%-%-* (.-) %-%-%-* (.-)$")
					if game_title ~= nil then
						format_gamelist = 5
						game_id = game_title:gsub(" %(also.-%)","")
						game_id = game_id:gsub(":"," -")
						game_id = game_id:gsub("/","-")
					else
						print("Error Gamelist Format. Not 5 and not 6")
						return nil
					end
				end
				print("Gamelist Format: "..tostring(format_gamelist))
			elseif format_gamelist == 5 then
				game_year, game_publisher, game_genre, game_player, game_title = line:match("(.-) %-%- (.-) %-%-%-* (.-) %-%-%-* (.-) %-%-%-* (.-)$")
				if game_title ~= nil then
					game_id = game_title:gsub(" %(also.-%)","")
					game_id = game_id:gsub(":"," -")
					game_id = game_id:gsub("/","-")
				end
			elseif format_gamelist == 6 then
				game_id, game_year, game_publisher, game_genre, game_player, game_title = line:match("(.-) %-* (%w-) %-%- (.-) %-%-%-* (.-) %-%-%-* (.-) %-%-%-* (.-)$")
			end
			if game_title == nil then
				-- print("\tInvalid gamelist data: ", line)
			else
				game_list[game_id] = {game_year, game_publisher, game_genre, game_player, game_title}
				no = no + 1
			end
		end
	end
	print("Gamelist Result: "..tostring(no))
	return game_list
end

-- url=https://archive.org/download/cylums-nintendo-ds-rom-collection/cylums-nintendo-ds-rom-collection_files.xml
-- return table of game list
function cylum_load_files_xml(url)
	local file_list, no, format_gamelist
	
	print("Downloading _files.xml ...", url)
	local rc, headers, content = http.request(url)
	if rc ~= 0 then
		print("Error: "..http.error(rc), rc)
		return nil
	end
	
	print("Importing _files.xml ...")
	file_list = {}
	no = 0
    for file_id, file_size in content:gmatch('<file name="(.-)".-<size>(.-)</size>') do
        file_id = file_id:match("^.+/(.+)$") or file_id
        file_id = file_id:gsub("&amp;", "&")
        file_list[http.escape(file_id)] = {file_size}
        no = no + 1
	end
	print("_files.xml Result: "..tostring(no))
	return file_list
end

-- user_url=https://archive.org/download/cylums-final-burn-neo-rom-collection/Cylum%27s%20FinalBurn%20Neo%20ROM%20Collection%20%2802-18-21%29/
-- user_url=https://archive.org/download/cylums-snes-rom-collection/Cylum%27s%20SNES%20ROM%20Collection%20%2802-14-2021%29.zip/
-- user_url=https://archive.org/download/cylums-snes-rom-collection
-- user_url=https://archive.org/details/cylums-snes-rom-collection
function cylum_archive_generate_db(user_url, category)
	local fo, output_fname, rc, headers, content, nn, list, list2, game_data, content_format,w1,w2,w3,w4, user_url2
	
	if user_url == "" or user_url == nil then
		return false
	end
	
	print("\nProcess url: "..user_url)
	category = category or "general"
	
	if user_url:sub(-1,-1) == "/" then	-- request 2 level url "https://archive.org/download/cylums-snes-rom-collection/Cylum%27s%20SNES%20ROM%20Collection%20%2802-14-2021%29.zip/"
		user_url2 = user_url
		output_fname = user_url:match('download/(.-)/')
	else -- request 1 level url "https://archive.org/details/cylums-snes-rom-collection"
		if user_url:match('/details/') then
			user_url = user_url:gsub('/details/','/download/')
			output_fname = user_url:match('download/(.-)$')
		elseif user_url:match('/download/') then
			output_fname = user_url:match('download/(.-)$')
		else
			output_fname = user_url
			user_url = 'https://archive.org/download/'..user_url
		end
		
		content_format = nil
		rc, headers, content = http.request(user_url)
		if rc ~= 0 then
			print("Error: "..http.error(rc), rc)
			return false
		end
		
		-- guess content format based on url found
		for link,desc in content:gmatch('<td><a href="(.-)">(.-)</a>') do
			if desc:sub(-1,-1) == "/" then
				-- print(link,desc)
				print("Content format: folder")
				content_format = content_format_folder
				user_url2 = user_url.."/"..link
			elseif desc:sub(-4,-1) == ".zip" then
				-- print(link,desc)
				print("Content format: zipped")
				content_format = content_format_zipped
				user_url2 = user_url.."/"..link.."/"
			end
		end	
	end
	
	if output_fname == nil then
		print("Error input url.\nExample: https://archive.org/download/cylums-final-burn-neo-rom-collection/Cylum%27s%20FinalBurn%20Neo%20ROM%20Collection%20%2802-18-21%29/")
		return false
	end

	if content_format == content_format_folder then
		list2 = cylum_load_files_xml("https://archive.org/download/"..output_fname.."/"..output_fname.."_files.xml")
		if list2 == nil then
			print("[ERROR] Can not find _files.xml")
			return false
		end
	end

	list = cylum_load_gamelist("https://archive.org/download/"..output_fname.."/Gamelist.txt")
	if list == nil then
		return false
	end
	
	if user_url2 == "https://archive.org/download/cylums-playstation-rom-collection/test/" then
		user_url2 = "https://archive.org/download/cylums-playstation-rom-collection/Cylum%27s%20PlayStation%20ROM%20Collection%20%2802-22-2021%29/"
	end
	print("Downloading database content from url2...")
	print("url2: "..user_url2)
	rc, headers, content = http.request(user_url2)
	if rc ~= 0 then
		print("Error: "..http.error(rc), rc)
		return false
	end

	if content_format == nil then
		local title
		title = content:match("<title>(.-)</title>"):sub(-7,-1)	-- guess content format based on TITLE
		if title == "Archive" then
			print("Content format: zipped")
			content_format = content_format_zipped
		elseif title == "listing" then
			print("Content format: folder")
			content_format = content_format_folder

            if list2 == nil then
                list2 = cylum_load_files_xml("https://archive.org/download/"..output_fname.."/"..output_fname.."_files.xml")
                if list2 == nil then
                    print("[ERROR] Can not find _files.xml")
                    return false
                end
            end
		else
			print('Error unknown content format: not zipped and not folder')
			return false
		end
	end

	-- count char / in user_url2 to generate output_fname (special for sub folder)
	local charcount = 0
	for i=1, #user_url2 do
		if user_url2:sub(i, i) == "/" then
			charcount = charcount + 1
		end
	end
	
	if charcount == 7 then	-- process sub folder
		local subfolder = user_url2:match(".*/(.-)/$")
		subfolder = subfolder:gsub('%%(%x%x)', function(hex)
			return string.char(tonumber(hex, 16))
		end)
		subfolder = subfolder:gsub("%W","_")
		
		output_fname = output_fname:gsub("%W","_").."_"..subfolder..".csv"
	else -- process base folder
		output_fname = output_fname:gsub("%W","_")..".csv"
	end

	print("Generating database "..output_fname.." ...")
	fo = io.open(DB_PATH.."/"..output_fname, "w")
	if fo == nil then
		print('Error open a file '..output_fname)
		return false
	end	
	fo:write("#category="..category.."\n")
	fo:write("#url="..user_url2.."\n")
	nn = 0
	for w1,w2,w3 in content:gmatch(content_format) do
		if w1:match("%.jpg$") or w1:match("%.torrent$") or w1:match("%.xml$") or w1:match("%.sqlite$") 
		or w1:match("^/details/") or w2:match("parent directory")
		then
			-- print("\tIgnore: ", w1)
		elseif w3:match("%-") then
			print("Process sub folder: "..w1)
			cylum_archive_generate_db(user_url2..w1, category)
		else
			w2 = w2:gsub("&amp;", "&")
			w2 = w2:gsub("%.%w+$", "")
			w2 = w2:gsub(".-/", "")
			-- print("w2: ", w2)
			game_data = list[w2]

            if list2 == nil then
                w4 = w3
            else
                local flist = list2[w1]
                if flist == nil then
                    print("[WARNING] Unmapped size for ", w1)
                    w4 = "--"
                else
                    w4 = flist[1]
                end
            end
            
			if is_string_all_digit(w3) then
				w3 = format_bytes(w3)
			end
			if game_data == nil then
				fo:write(string.format("%s|%s|%s|%s\n",w1, w2, w3, w4))
			else
				fo:write(string.format("%s|%s|%s - %s/%s - (c)%s %s|%s\n", w1, game_data[5], w3, game_data[3], game_data[4], game_data[1], game_data[2], w4))
			end
			nn = nn + 1
		end
	end
	fo:close()
	print("Result created db: "..nn.."\n")
	return true
end

function general_archive_generate_db(user_url, category)
	local fo, output_fname, rc, headers, content, nn, list2, w4
	if user_url == "" or user_url == nil then
		return false
	end
	
	category = category or "general"
	if user_url:match('/details/') then
		user_url = user_url:gsub('/details/','/download/')
		output_fname = user_url:match('download/(.-)$')
	elseif user_url:match('/download/') then
		output_fname = user_url:match('download/(.-)$')
	else
		output_fname = user_url
		user_url = 'https://archive.org/download/'..user_url
	end
	list2 = cylum_load_files_xml("https://archive.org/download/"..output_fname.."/"..output_fname.."_files.xml")
	if list2 == nil then
		print("[ERROR] Can not find "..output_fname.."_files.xml")
		return false
	end
	output_fname = output_fname:gsub("%W","_")..".csv"
	
	fo = io.open(DB_PATH.."/"..output_fname, "w")
	if fo == nil then
		print('Error open a file '..output_fname)
		return false
	end
	print("Downloading url="..user_url)
	rc, headers, content = http.request(user_url)
	if rc ~= 0 then
		print("Error: "..http.error(rc), rc)
		return false
	end
	
	fo:write("#category="..category.."\n")
	fo:write("#url="..user_url.."\n")
	nn = 0
	print("Extracting data...")
	for w1,w2,w3 in content:gmatch(content_format_folder) do
		if w1:match("%.jpg$") or w1:match("%.torrent$") or w1:match("%.xml$") or w1:match("%.sqlite$") 
		or w3:match("%-") or w1:match("^/details/") or w2:match("parent directory")
		then
			print("Ignore: ", w1)
		else
			local flist = list2[w1]
			if flist == nil then
				print("[WARNING] Unmapped size for ", w1)
				w4 = "--"
			else
				w4 = flist[1]
			end
			w2 = w2:gsub("&amp;", "&")
			w2 = w2:gsub("%.%w+$", "")
			w2 = w2:gsub(".-/", "")
			fo:write(w1.."|"..w2.."|"..w3.."|"..w4.."\n")
			nn = nn + 1
		end
	end
	fo:close()
	print("Successfully process "..nn.." data")
	return true
end

function general_archive_generate_db_with_dict(user_url, category)
	local fo, output_fname, rc, headers, content, nn, list1, list2, w4
	if user_url == "" or user_url == nil then
		return false
	end
	
	category = category or "general"
	if user_url:match('/details/') then
		user_url = user_url:gsub('/details/','/download/')
		output_fname = user_url:match('download/(.-)$')
	elseif user_url:match('/download/') then
		output_fname = user_url:match('download/(.-)$')
	else
		output_fname = user_url
		user_url = 'https://archive.org/download/'..user_url
	end
	list2 = cylum_load_files_xml("https://archive.org/download/"..output_fname.."/"..output_fname.."_files.xml")
	if list2 == nil then
		print("[ERROR] Can not find "..output_fname.."_files.xml")
		return false
	end
	output_fname = output_fname:gsub("%W","_")..".csv"

	-- load gamelist from dict
	local dict = "db/"..category..".gamelist.txt"
	print("Loading dict from "..dict)
	list1 = {}
	local data,header = csv.reader(dict,'|')
	for row in data:rows() do
		list1[row[1]] = {row[2], row[3]}
	end
	
	fo = io.open(DB_PATH.."/"..output_fname, "w")
	if fo == nil then
		print('Error open a file '..output_fname)
		return false
	end
	print("Downloading url="..user_url)
	rc, headers, content = http.request(user_url)
	if rc ~= 0 then
		print("Error: "..http.error(rc), rc)
		return false
	end
	
	fo:write("#category="..category.."\n")
	fo:write("#url="..user_url.."\n")
	nn = 0
	print("Extracting data...")
	for w1,w2,w3 in content:gmatch(content_format_folder) do
		if w1:match("%.jpg$") or w1:match("%.torrent$") or w1:match("%.xml$") or w1:match("%.sqlite$") 
		or w3:match("%-") or w1:match("^/details/") or w2:match("parent directory")
		then
			print("Ignore: ", w1)
		else
			w2 = w2:gsub("&amp;", "&")
			w2 = w2:gsub("%.%w+$", "")
			w2 = w2:gsub(".-/", "")

			local flist1 = list1[w2]
			if flist1 == nil then
					print("[WARNING] Unmapped gamelist for ", w2)
			else
				if flist1[1] == nil then
					print("[WARNING] Invalid data(nil) for ", w2)
				else
					w2 = flist1[1]
				end
				if flist1[2] == nil then
					print("[WARNING] Invalid data(nil) for ", w2)
				else
					w3 = w3 .. "/"..flist1[2]
				end
			end
			
			local flist2 = list2[w1]
			if flist2 == nil then
				print("[WARNING] Unmapped size for ", w1)
				w4 = "--"
			else
				w4 = flist2[1]
			end
			
			if flist1 ~= nil then
				fo:write(w1.."|"..w2.."|"..w3.."|"..w4.."\n")
				nn = nn + 1
			end
		end
	end
	fo:close()
	print("Successfully process "..nn.." data")
	return true
end

-- origin_url = "https://archive.org/download/cylums-snes-rom-collection"
-- origin_url = "https://archive.org/details/cylums-snes-rom-collection"
-- origin_url = "https://archive.org/download/cylums-snes-rom-collection/Cylum%27s%20SNES%20ROM%20Collection%20%2802-14-2021%29.zip/"

-- cylum_archive_generate_db("https://archive.org/details/cylums-final-burn-neo-rom-collection", "fbneo")
-- cylum_archive_generate_db("https://archive.org/details/cylums-snes-rom-collection","snes")
-- cylum_archive_generate_db("https://archive.org/details/cylums-nintendo-ds-rom-collection","nds")
-- cylum_archive_generate_db("https://archive.org/details/cylums-game-boy-advance-rom-collection_202102","gba")
-- cylum_archive_generate_db("https://archive.org/details/cylums-sega-genesis-rom-collection","megadrive")
-- cylum_archive_generate_db("https://archive.org/details/cylums-nes-rom-collection","nes")
-- cylum_archive_generate_db("https://archive.org/details/cylums-neo-geo-rom-collection","neogeo")
-- cylum_archive_generate_db("https://archive.org/details/cylums-playstation-rom-collection","psx")
-- cylum_archive_generate_db("https://archive.org/details/cylums-playstation-rom-collection","psx")
-- cylum_archive_generate_db("https://archive.org/details/cylums-turbo-grafx-cd-rom-collection","tg-cd")

-- general_archive_generate_db("https://archive.org/download/Nintendo64V2RomCollectionByGhostware","n64")
-- general_archive_generate_db("https://archive.org/details/WiiWareCollectionByGhostware","wii")
-- general_archive_generate_db("https://archive.org/details/SegaDreamcastPALRomCollectionByGhostware","dreamcast")
-- general_archive_generate_db("https://archive.org/details/CapcomCPS2ByDataghost","cps2")
-- general_archive_generate_db("https://archive.org/details/SegaSaturnRomCollectionByGhostware","saturn")
-- general_archive_generate_db("https://archive.org/details/NaomiRomsReuploadByGhostware","naomi")
-- general_archive_generate_db("https://archive.org/details/AtomiswaveReuploadByGhostware","atomiswave")
-- https://archive.org/download/mame-0.221-roms-merged


if #arg < 2 then
    print("Usage: luaxx gen_db.lua <source_url> <target_directory>")
    return
end

-- Validate and use the arguments
if arg[1] and arg[2] then
    print("Source URL: " .. arg[1])
    print("Target Directory: " .. arg[2])
else
	print("Usage: luaxx gne_db.lua <source_url> <target_directory>")
    print("Invalid arguments")
	return
end

http.set_conf(http.OPT_TIMEOUT, 90)
-- general_archive_generate_db(arg[1], arg[2])
general_archive_generate_db_with_dict(arg[1], arg[2])

-- function load_file(filename)
-- 	local fi, content

-- 	fi = io.open(filename, "r")
-- 	if fi == nil then
-- 		print("[error] Load file "..filename)
-- 		return nil
-- 	end
-- 	content = fi:read("*a")
-- 	fi:close()
-- 	return content
-- end
-- content = load_file("input.xml")
-- for c1,c2 in content:gmatch('<file name="(.-)".-<size>(.-)</size>') do
--     c1 = c1:match("^.+/(.+)$") or c1
--     print(http.escape(c1), c2)
-- end
-- http.request{url = 'https://archive.org/download/cylums-nintendo-ds-rom-collection/cylums-nintendo-ds-rom-collection_files.xml', output_filename = 'input.xml'}
-- http.request{url = target_url, output_filename = 'input2.htm'}
-- n = 0
-- for line in io.lines("input.htm") do
--     c1,c2 = line:match('^<tr><td><a href=".-">(.-)<.-"size">(.-)<')
--     if c1 ~= nil then
--         n = n + 1
--         print(c1, c2)
--     end
-- end
-- print('Found ', n)

