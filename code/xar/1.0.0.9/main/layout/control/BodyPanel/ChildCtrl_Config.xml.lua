local tipUtil = XLGetObject("GS.Util")
local tFunctionHelper = XLGetGlobal("GreenWallTip.FunctionHelper")
local g_bHasInit = false
local g_tConfigElemList = {
	[1] = {"AutoStup", "开机时启动",  function(self,eventname) StateChange_AutoStup(self,eventname) end},
	[2] = {"AutoUpdate", "自动检查更新", function(self,eventname) StateChange_AutoUpdate(self,eventname) end},
	[3] = {"HideMainPage", "启动后隐藏主界面", function(self,eventname) StateChange_HideMain(self,eventname) end},
	[4] = {"BubbleRemind", "使用气泡提示", function(self,eventname) StateChange_Bubble(self,eventname) end},
	[5] = {"FiltInTime", "开启实时过滤", function(self,eventname) StateChange_FiltInTime(self,eventname) end},
}

local g_tReportState = {}

---方法---
function OnShowPanel(self, bShow)
	if not g_bHasInit then
		InitConfigCtrl(self)
	end
end


function SetElemState(self, strElemKey, bState)
	if not IsRealString(strElemKey) or type(bState) ~= "boolean" then
		return 
	end
	
	for nIndex, tElemInfo in pairs(g_tConfigElemList) do
		local strKeyName = tElemInfo[1]
		if strElemKey == strKeyName then
			local objElem = self:GetControlObject(strElemKey)
			if not objElem then 
				return			
			end
			
			objElem:SetSwitchState(bState)
			g_tReportState[nIndex] = bState
		end
	end
end

---事件---
function OnMouseEnterBkg(self)
	local objBkg = self	
	objBkg:SetTextureID("Greenwall.ConfigElem.ElemBkg.Hover")
end

function OnMouseLeaveBkg(self)
	local objBkg = self	
	objBkg:SetTextureID("Greenwall.ConfigElem.ElemBkg.Normal")
end

function OnMouseEnterBtn(self)
	self:RouteToFather()
end

function OnMouseLeaveBtn(self)
	self:RouteToFather()
end


function OnInitControl(self)
	InitConfigCtrl(self)
end


function OnDestroy(self)
	local tState = {}
	
	for nIndex, bState in ipairs(g_tReportState) do
		if bState then
			tState[nIndex] = 1
		else
			tState[nIndex] = 0
		end	
	end
	
	local tStatInfo = {}
	local strStateList = table.concat(tState, "")
	tStatInfo.strEC = "ConfigPanel"
	tStatInfo.strEA = "userstate"
	tStatInfo.strEL = strStateList

	SendReport(tStatInfo)
end

----------
function InitConfigCtrl(objRootCtrl)
	objFather = objRootCtrl:GetControlObject("ChildCtrl_Config.MainWnd.Container")
	if objFather == nil then
		return nil
	end
	
	local objFactory = XLGetObject("Xunlei.UIEngine.ObjectFactory")
	if objFactory == nil then
		return nil
	end
	
	local nFatherLeft, nFatherTop, nFatherRight, nFatherBottom = objFather:GetObjPos(objElem)
	local nFatherHeight = nFatherBottom - nFatherTop
	local nElemHeight = 35
	local nElemSpace = 10
	local nExtraSpace = 5

	local tUserCfg = GetConfigTable() or {}
	
	--依序创建
	for nIndex, tElemInfo in ipairs(g_tConfigElemList) do
		local strElemKey = tElemInfo[1]
		local strElemText = tElemInfo[2]
		local fnElemEvent = tElemInfo[3]
	
		local objElem = objFactory:CreateUIObject(strElemKey, "ConfigElem")
		if nil == objElem then
			return false
		end
		
		local bState = FetchValueByPath(tUserCfg, {"tConfig", strElemKey, "bState"})
		if bState == nil then
			bState = true
		end
		
		objFather:AddChild(objElem)
		objElem:SetElemText(strElemText)
		objElem:AttachListener("OnStateChange", false, fnElemEvent)
		objElem:SetSwitchState(bState)
		g_tReportState[nIndex] = bState
		
		local nNewTop = nElemSpace + (nIndex-1)*(nElemHeight+nElemSpace) + nExtraSpace
		objElem:SetObjPos(0, nNewTop, "father.width", nNewTop+nElemHeight)
	end
	
	SetFiltConfigState(objRootCtrl)
	SetAutoStupState(objRootCtrl)
	g_bHasInit = true
end

---------------
--开机启动
function StateChange_AutoStup(self, eventname)
	local bState = self:GetSwitchState()
	SaveStateToCfg("AutoStup", bState)
	g_tReportState[1] = bState
	
	local FunctionObj = XLGetGlobal("GreenWallTip.FunctionHelper")
	local bHasAutoStup = false
	local strGreenShieldPath = FunctionObj.RegQueryValue("HKEY_LOCAL_MACHINE\\Software\\GreenShield\\path")
	
	local strRegStupList = {
		"HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield",
		"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield",
	}
	
	for _, strRegPath in pairs(strRegStupList) do 
		local szCmdLine = FunctionObj.RegQueryValue(strRegPath) or ""
		if IsRealString(szCmdLine) 
			and string.find(string.lower(szCmdLine), string.lower(tostring(strGreenShieldPath)), 1, true) then
			bHasAutoStup = true  -- 已经开机启动
			break
		end
	end
	
	if bState and not bHasAutoStup then   --bState == true 表示设置开机启动
		if IsRealString(strGreenShieldPath) and tipUtil:QueryFileExists(strGreenShieldPath) then
			local sCommandline = "\""..strGreenShieldPath.."\"".." /embedding /sstartfrom sysboot "
			bRetCode = FunctionObj.RegSetValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield", sCommandline)
		end
		
	elseif bHasAutoStup then
		RegDeleteValue("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield")
	end
end


function RegDeleteValue(sPath)
	if IsRealString(sPath) then
		local sRegRoot, sRegPath = string.match(sPath, "^(.-)[\\/](.*)")
		if IsRealString(sRegRoot) and IsRealString(sRegPath) then
			return tipUtil:DeleteRegValue(sRegRoot, sRegPath)
		end
	end
	return false
end


--检查更新
function StateChange_AutoUpdate(self, eventname)
	local bState = self:GetSwitchState()
	SaveStateToCfg("AutoUpdate", bState)
	g_tReportState[2] = bState
end

--启动后隐藏主界面
function StateChange_HideMain(self, eventname)
	local bState = self:GetSwitchState()
	SaveStateToCfg("HideMainPage", bState)
	g_tReportState[3] = bState
end

--使用气泡提示
function StateChange_Bubble(self, eventname)
	local bState = self:GetSwitchState()
	SaveStateToCfg("BubbleRemind", bState)
	g_tReportState[4] = bState
end

--开启实时过滤
function StateChange_FiltInTime(self, eventname)
	local bState = self:GetSwitchState()
	SaveStateToCfg("FiltInTime", bState)
	g_tReportState[5] = bState
	SaveStateToFile(bState)
end


function SaveStateToFile(bNewState)
	local strStartCfgPath = tFunctionHelper.GetCfgPathWithName("startcfg.ini")
	if not IsRealString(strStartCfgPath) then
		return
	end

	if bNewState then
		tipUtil:WriteINI("pusher", "noremind", 0, strStartCfgPath)
	else
		tipUtil:WriteINI("pusher", "noremind", 1, strStartCfgPath)
		tipUtil:WriteINI("pusher", "lastpull", tipUtil:GetCurrentUTCTime(), strStartCfgPath)
	end	
end


function SetFiltConfigState(objRootCtrl)
	local objFiltInTime = objRootCtrl:GetControlObject("FiltInTime")
	if not objFiltInTime then
		return
	end
	
	local strStartCfgPath = tFunctionHelper.GetCfgPathWithName("startcfg.ini")
	if not IsRealString(strStartCfgPath) then
		return
	end

	local nLastPull, bRet = tipUtil:ReadINI(strStartCfgPath, "pusher", "lastpull")
	local nSpanDay, bRet = tipUtil:ReadINI(strStartCfgPath, "pusher", "noremindspanday") 
	if tonumber(nLastPull) == nil then
		nLastPull = 0
	end
	if tonumber(nSpanDay) == nil then
		nSpanDay = 7
	end

	local nSpanSec = nSpanDay*24*3600
	local nCurTimeUTC = tipUtil:GetCurrentUTCTime()
	if math.abs(nCurTimeUTC-nLastPull) > nSpanSec then
		objFiltInTime:SetSwitchState(true)
	end	
end


function SetAutoStupState(objRootCtrl)
	local objAutoStup = objRootCtrl:GetControlObject("AutoStup")
	if not objAutoStup then
		return
	end
	
	local FunctionObj = XLGetGlobal("GreenWallTip.FunctionHelper")
	local bHasAutoStup = false
	local strGreenShieldPath = FunctionObj.RegQueryValue("HKEY_LOCAL_MACHINE\\Software\\GreenShield\\path")
	
	local strRegStupList = {
		"HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield",
		"HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run\\GreenShield",
	}
	
	for _, strRegPath in pairs(strRegStupList) do 
		local szCmdLine = FunctionObj.RegQueryValue(strRegPath) or ""
		if IsRealString(szCmdLine) 
			and string.find(string.lower(szCmdLine), string.lower(tostring(strGreenShieldPath)), 1, true) then
			bHasAutoStup = true  -- 已经开机启动
			break
		end
	end
	
	objAutoStup:SetSwitchState(bHasAutoStup)
end

---------------

-------------ConfigElemCtrl---------
function OnClickSwitchButton(self)
	local objOwner = self:GetOwnerControl()
	local bState = objOwner:GetSwitchState()
	local bNewState = not bState
	
	objOwner:SetSwitchState(bNewState)
	objOwner:FireExtEvent("OnStateChange")
end


function GetSwitchState(self)
	local attr = self:GetAttribute()
	local bState = attr.SwitchState 
	return bState
end


function SetSwitchState(self, bState)
	if type(bState) ~= "boolean" then
		return
	end

	local objSwitch = self:GetControlObject("ConfigElem.SwitchBtn")
	if nil == objSwitch then
		return 
	end
	
	if bState then
		objSwitch:SetTextureID("GreenWall.Common.SwitchButton.Open")
	else
		objSwitch:SetTextureID("GreenWall.Common.SwitchButton.Close")
	end
	
	local attr = self:GetAttribute()
	attr.SwitchState = bState
	
	--self:FireExtEvent("OnStateChange")
end


function SetElemText(self, strText)
	local objText = self:GetControlObject("ConfigElem.Text")
	if nil == objText then
		return 
	end
	
	objText:SetText(strText)
end

-------------辅助函数------------
function SendReport(tStatInfo)
	local FunctionObj = XLGetGlobal("GreenWallTip.FunctionHelper")
	FunctionObj.TipConvStatistic(tStatInfo)
end


function GetConfigTable()
	if type(tFunctionHelper.ReadConfigFromMemByKey) ~= "function" then
		return nil
	end
	
	local tUserConfig = tFunctionHelper.ReadConfigFromMemByKey("tUserConfig")
	return tUserConfig	
end

function SaveConfigTable()
	if type(tFunctionHelper.SaveConfigToFileByKey) ~= "function" then
		return
	end
	
	tFunctionHelper.SaveConfigToFileByKey("tUserConfig")
end


function SaveStateToCfg(strKeyName, bState)
	local tUserCfg = GetConfigTable() or {}
	if type(tUserCfg["tConfig"]) ~= "table" then
		tUserCfg["tConfig"] = {}
	end
	
	if type(tUserCfg["tConfig"][strKeyName]) ~= "table" then
		tUserCfg["tConfig"][strKeyName] = {}
	end
	
	tUserCfg["tConfig"][strKeyName]["bState"] = bState
	SaveConfigTable()
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


function IsRealString(AString)
    return type(AString) == "string" and AString ~= ""
end


