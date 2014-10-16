local tipUtil = XLGetObject( "GS.Util" )

function OnCreate( self )
	local workleft, worktop, workright, workbottom = tipUtil:GetWorkArea()
	local selfleft, selftop, selfright, selfbottom = self:GetWindowRect()
	local wndwidth, wndheight = selfright - selfleft, selfbottom - selftop
	local objtree = self:GetBindUIObjectTree()
	local objRootCtrl = objtree:GetUIObject("root.layout")
	local webleft, webtop, webright, webbottom = objRootCtrl:GetAbsPos()
	local webwidth, webheight = webright - webleft, webbottom - webtop
	local wndleft = ((workright-workleft)-webwidth)/2-webleft
	local wndtop = ((workbottom-worktop)-webheight)/2-webtop
	self:Move(wndleft, wndtop, wndwidth, wndheight)
	
	SetVersionText(objRootCtrl)
end

function OnClickCloseBtn(self)
	local objTree = self:GetOwner()
	local objHostWnd = objTree:GetBindHostWnd()
	objHostWnd:Show(0)
end

function SetVersionText(objRootCtrl)
	local objVersion = objRootCtrl:GetObject("TipAbout.MainWndCenter.Bkg:TipAbout.MainWndCenter.Version")
	if not objVersion then
		return
	end
	
	local FunctionObj = XLGetGlobal("GreenWallTip.FunctionHelper")
	local strVersion = FunctionObj.GetGSVersion()
	local strText = "版本号 ："..tostring(strVersion).." 正式版本"
	objVersion:SetText(strText)
end
