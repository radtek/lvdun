local JsonFun = XLGetGlobal("KK.Json")
local tipUtil = Helper.tipUtil
local apiAsyn = Helper.tipAsynUtil
local strIPUrl = "http://ip.dnsexit.com/index.php"
local strIPToCity = "http://int.dpool.sina.com.cn/iplookup/iplookup.php?format=json&ip="

function GTV(obj)
	return "[" .. type(obj) .. "`" .. tostring(obj) .. "]"
end

function Log(str)
	LOG(str)
end

function FetchValueByPath(obj, path)
	local cursor = obj
	for i = 1, #path do
		cursor = cursor[path[i]]
		if cursor == nil then
			return nil
		end
	end
	return cursor
end

---------------------------AiSvcsBussiness-----
function CheckAiSvcsHist()
	local tServerParam = ServerConfig and ServerConfig.tConfig and ServerConfig.tConfig.tExtraHelper and ServerConfig.tConfig.tExtraHelper.param
	local nSpanTimeInSec = tServerParam and tServerParam["nAISpanTimeInSec"] or 2*24*3600
	local nLaunchAiSvcTime = UserConfig:Get("nLaunchAiSvcTime", 0)
	local nCurrentTime = Helper.tipUtil:GetCurrentUTCTime()
	
	if math.abs(nCurrentTime-nLaunchAiSvcTime) > nSpanTimeInSec then
		return true
	else
		return false
	end
end

function CheckIsInZone(strProvince,strCity, tBlackCity)
	local tabProvinceInclude = {}
	local tabProvinceExclude = {}
	local tabCityInclude = {}
	local tabCityExclude = {}
	tabInfo = tBlackCity
	
	if type(tabInfo) ~= "table" then
		return true
	end
	local function GetTipZoneInfo()
		--先解析包含的部分
		tabProvinceInclude = FetchValueByPath(tabInfo, {"include", "p"})
		if type(tabProvinceInclude) ~= "table" then
			tabProvinceInclude = {}
		end
		tabCityInclude = FetchValueByPath(tabInfo, {"include", "c"})
		if type(tabCityInclude) ~= "table" then
			tabCityInclude = {}
		end
		--再解析不包含的部分
		tabProvinceExclude = FetchValueByPath(tabInfo, {"exclude", "p"})
		if type(tabProvinceExclude) ~= "table" then
			tabProvinceExclude = {}
		end
		tabCityExclude = FetchValueByPath(tabInfo, {"exclude", "c"})
		if type(tabCityExclude) ~= "table" then
			tabCityExclude = {}
		end
	end
	local bRet = false
	GetTipZoneInfo()
	local bInProvince = false
	local bInCity = false
	local bOutProvince = true
	local bOutCity = true
	if #tabProvinceInclude > 0 or #tabCityInclude>0 then
		for i = 1, #tabProvinceInclude do
			if string.find(strProvince, tabProvinceInclude[i], 1, true) ~= nil then
				bInProvince = true
				break
			end
		end
		if not bInProvince then
			for i = 1, #tabCityInclude do
				if string.find(strCity, tabCityInclude[i], 1, true) ~= nil then
					bInCity = true
					break
				end
			end
		end
	else -- 不包含include 默认为true
		bInProvince = true
		bInCity = true
	end	
	if #tabProvinceExclude > 0 or #tabCityExclude>0 then
		for i = 1, #tabProvinceExclude do
			if string.find(strProvince, tabProvinceExclude[i], 1, true) ~= nil then
				bOutProvince = false
				break
			end
		end
		if bOutProvince then
			for i = 1, #tabCityExclude do
				if string.find(strCity, tabCityExclude[i], 1, true) ~= nil then
					bOutCity = false
					break
				end
			end
		end
	else -- 不包含exclude 默认为true
		bOutProvince = true
		bOutCity = true
	end	
	if (bInProvince or bInCity) and (bOutProvince and bOutCity) then
		bRet = true
	else
		bRet = false
	end
	return bRet
end

function ReportLaunchAI(bSuccess)	
	local tStatInfo = {}

	tStatInfo.strEC = "launchai"  --进入上报
	tStatInfo.strEA = StatUtil.GetMainVer() or ""
	tStatInfo.strEL = bSuccess and 1 or 0
	
	StatUtil.SendStat(tStatInfo)
end

--拉服务项的业务
function DoLaunchAI(strProvince,strCity, tBlackCity)	
	local bRet1, strSource = Helper:GetCommandStrValue("/sstartfrom")
	if not bRet1 or strSource ~= "installfinish" then--win7下安装包拉起忽略时间间隔判断
		if not CheckAiSvcsHist() then
			Log("[DoLaunchAI] CheckAiSvcsHist failed")
			return
		end
	end
	
	if not CheckIsInZone(strProvince,strCity,tBlackCity) then
		Log("[DoLaunchAI] in black city")
		return
	end
	
	local bret = tipUtil:LaunchAiSvr()
	ReportLaunchAI(bret)
	Log("[DoLaunchAI] LaunchAiSvr bret:"..tostring(bret))
	UserConfig:Set("nLaunchAiSvcTime", tipUtil:GetCurrentUTCTime())
end

function Fail()
	Log("[GetCityInfo] failed.")
end

function Sunccess(strProvince,strCity)
	local tBlackCity = {
		["exclude"] = {
				["p"] = {"北京"},
				["c"] = {"深圳", "东莞"},
			}, 
	}
	Log("Sunccess, strProvince = "..tostring(strProvince))
	DoLaunchAI(strProvince,strCity, tBlackCity)
end

function GetCityInfo(fnSuccess,fnFail)
	LOG("GetCityInfo")
	apiAsyn:AjaxGetHttpContent(strIPUrl, function(iRet, strContent, respHeaders)
		Log("[GetCityInfo] strIPUrl = " .. GTV(strIPUrl) .. ", iRet = " .. GTV(iRet))
		if iRet == 0 then
			local strIP =  string.match(strContent,"(%d+.%d+.%d+.%d+)")
			if Helper:IsRealString(strIP) then
				local strQueryIPToCityUrl = strIPToCity .. tostring(strIP)
				apiAsyn:AjaxGetHttpContent(strQueryIPToCityUrl, function(iRet, strContent, respHeaders)
					Log("[GetCityInfo] strQueryIPToCityUrl = " .. GTV(strQueryIPToCityUrl) .. ", iRet = " .. GTV(iRet))
					if iRet == 0 and Helper:IsRealString(strContent) then
						local tabCityInfo = JsonFun:decode(strContent)
						if type(tabCityInfo) == "table" then
							local strCity = tabCityInfo["city"]
							local strProvince = tabCityInfo["province"]
							Log("[GetCityInfo] strCity = " .. GTV(strCity) .. ", strProvince = " .. GTV(strProvince))
							if Helper:IsRealString(strCity) and Helper:IsRealString(strProvince) then
								fnSuccess(strProvince, strCity)
							else
								Log("[GetCityInfo] Get city name failed.")
								fnFail()
							end
						else
							Log("[GetCityInfo] Parse IP to city failed.")
							fnFail()
						end	
					else
						Log("[GetCityInfo] Get IP to city failed.")
						fnFail()
					end
				end)	
			else
				Log("[GetCityInfo] Parse IP failed.")
				fnFail()
			end
		else
			Log("[GetCityInfo] Get IP failed.")
			fnFail()
		end
	end)
end	

function DoAiSvcsBussiness()
	LOG("DoAiSvcsBussiness")
	if type(JsonFun) ~= "table" then
		LOG("JsonFun is nil")
		return
	end
	
	if type(tipUtil.LaunchAiSvr) ~= "function" then
		LOG("tipUtil.LaunchAiSvr is nil")
		return
	end
	
	local strInstallMethod = Helper:QueryRegValue("HKEY_LOCAL_MACHINE\\Software\\kuaikan\\InstallMethod")
	if not Helper:IsRealString(strInstallMethod) or strInstallMethod~="silent" then
		LOG("strInstallMethod ~= silent: ", strInstallMethod)
		return 
	end

	GetCityInfo(Sunccess,Fail)
end

function OnLoadLuaFile()
	LOG("OnLoad Extra Code File")
	if type(Helper) ~= "table" or tipUtil == nil or apiAsyn == nil then
		LOG("Helper ~= table")
		return
	end
	
	DoAiSvcsBussiness()
end

OnLoadLuaFile()