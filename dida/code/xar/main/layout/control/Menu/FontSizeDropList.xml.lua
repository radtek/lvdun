local tFunHelper = XLGetGlobal("DiDa.FunctionHelper")
local Helper = XLGetGlobal("Helper")
local tipUtil = tFunHelper.tipUtil

function SetDefaultItemHover(self)
	-- local objNormalMenu = self:GetControlObject("Menu.Context")
	-- local objMenuContainer = objNormalMenu:GetControlObject("context_menu")
	
	-- local nBeginYear, nEndYear = tFunHelper.GetYearScale()
	-- local nCurYear = GetYearFromComboBox(objMenuContainer)
	-- local nCurDiff = nCurYear - nBeginYear
	
	-- local nShowDiff = nCurDiff   --展示的时候，一并显示前三年的数字
	-- if nCurDiff > 3 then
		-- nShowDiff = nCurDiff-3
	-- end
	
	-- local nItemHieght = objNormalMenu:GetItemHeight()
	-- local nScrollPos = nShowDiff*nItemHieght

	-- objNormalMenu:SetScrollPos(nScrollPos)
	-- objNormalMenu:MoveItemListPanel(nScrollPos)
	
	-- local objChild = objMenuContainer:GetItem(nCurDiff+1)
	-- objMenuContainer:SetHoverItem(objChild, false)
end

function GetFontSizeConfig()
	local configPath = tipUtil:ExpandEnvironmentStrings("%PUBLIC%").."\\DIDA\\ddnotepad\\defaultcfg.dat"
	local configPathAA = tipUtil:ExpandEnvironmentStrings("%PUBLIC%").."\\DIDA\\AA\\AA\\AA\\AA\\AA\\AA\\defaultcfg.dat"
	local configTable = Helper:LoadLuaTable(configPath)
	
	if "table" ~= type(configTable) then
		configTable = {}
		configTable.FontSize = {8, 9, 11, 12, 14, 16, 18, 20, 24, 28, 32}
		local ret = Helper:SaveLuaTable(configTable, configPath)
		-- XLMessageBox(tostring(ret))
	end
	
	Helper:Assert(configTable.FontSize, "configTable.FontSize is nil")
	return configTable.FontSize
end

function OnInitControl(self)
	--CreateMenuContainer
	local templateMananger = XLGetObject("Xunlei.UIEngine.TemplateManager")	
	local menuTemplate = templateMananger:GetTemplate("fontsizemenu.context", "ObjectTemplate")
	if menuTemplate == nil then
		Helper:Assert(false)
		return nil
	end
	objMenuContainer = menuTemplate:CreateInstance( "context_menu" )
	
	if not objMenuContainer then
		Helper:Assert(false)
		return false
	end
	
	local tabFontSize = GetFontSizeConfig()
	local nTotalCount = 0
	for index=1,#tabFontSize do
		local objMenuItem = CreateMenuItem(self, tostring(tabFontSize[index]))	
		if objMenuItem then
			objMenuContainer:AddChild(objMenuItem)				
			nTotalCount = nTotalCount+1
		end				
	end

	BindMenuContainer(self, objMenuContainer, nMaxShowHistory, nTotalCount)
	SetDefaultItemHover(self:GetOwnerControl())
end

function OnMouseWheel(self, x, y, distance)
	local objNormalMenu = self:GetControlObject("Menu.Context")
	objNormalMenu:ProcessScrollWhenWheel(x, y, distance)
end

function CreateMenuItem(self, itemID)	
	local nBeginYear, nEndYear = tFunHelper.GetYearScale()
	local templateMananger = XLGetObject("Xunlei.UIEngine.TemplateManager")	
	local objMenuItemTempl = templateMananger:GetTemplate("menu.context.item", "ObjectTemplate")
	if objMenuItemTempl == nil then
		Helper:Assert(false)
		return nil
	end
	
	local objMenuItem = objMenuItemTempl:CreateInstance(tostring(itemID) )
	if not objMenuItem then
		Helper:Assert(false)
		return nil
	end

	local attr = objMenuItem:GetAttribute()
	attr.Text = tostring(itemID)
	
	local fontDropListCtrl = self:GetOwnerControl()
	objMenuItem:AttachListener("OnSelect", false, function() fontDropListCtrl:FireExtEvent("OnSelectFontSize", itemID) end)
	return objMenuItem
end

function BindMenuContainer(self, objMenuContainer, nMaxShowHistory, nTotalCount)
	local attr = self:GetAttribute()
	attr.nLinePerPage = 12
	attr.nTotalLineCount = nTotalCount

	self:OnInitControl(objMenuContainer)
end

function OnSelectFont(objMenuItem)
	--获取当前字号
	local strText = objMenuItem:GetText()
	
	--发事件,通知设置Edit字体
	local owner = objMenuItem:GetOwnerControl()
	owner:FireExtEvent("OnSelectFont", strText)
	
	--根据字体、字号，索引该Font是否已存在，不存在则创建字体
	
	--
	
	
	-- local objDateSelect = tFunHelper.GetMainCtrlChildObj("DiDa.CalendarView:DiDa.DateSelectCtrl")
	-- objDateSelect:SetYearText(strText)
	-- objDateSelect:ResetFestivalText()
	
	-- tFunHelper.UpdateCalendarContent()
end

function GetYearFromComboBox(objMenuContainer)
	local objDateSelect = tFunHelper.GetMainCtrlChildObj("DiDa.CalendarView:DiDa.DateSelectCtrl")
	if not objDateSelect then
		local strYear = os.date("%Y")
		return tonumber(strYear)
	end
	
	local objYearBox = objDateSelect:GetControlObject("Combobox.Year")

	local strText = objYearBox:GetText()
	if not IsRealString(strText) then
		return
	end
	local _, _, strYear = string.find(strText, "(%d*)[^%d]*")
	return tonumber(strYear)
end

function RouteToFather(self)
	self:RouteToFather()
end

function IsRealString(str)
	return type(str) == "string" and str ~= nil
end


