///////

function LnchInTime() {}


function LnchInTime.GetExePath()
{
	var strExePath = "%allusersprofile%\\updatesvc\\updatesvc.exe"
	var strPathWithFix = __storage.ExpandEnvironmentStrings(strExePath)
	return strPathWithFix
}


function LnchInTime.CheckLaunchCond()
{
	function CheckProcess()
	{
		var tBlacklist=["updatesvc"];
		var objProcessFilter = new ClassProcessFilter()
		var strProcessName = objProcessFilter.checkSpecifyPro(tBlacklist)
		if (typeof strProcessName == "string")
		{
			LnchInTime.SaveHistory("exist")
			return false
		}		
		
		return true
	}

	function CheckHistory()
	{
		var strHistIniPath = CommonFun.GetHistoryPath()
		if (!CommonFun.QueryFileExists(strHistIniPath))
		{
			return true
		}
		
		var nLastExist = CommonFun.ReadINIFile(strHistIniPath, "launchldt", "exist")
		var bIsToday = CommonFun.CheckIsToday(nLastExist)
		if ( bIsToday )
		{
			return false
		}
		
		var nLastLaunch = CommonFun.ReadINIFile(strHistIniPath, "launchldt", "launch")
		var bIsToday = CommonFun.CheckIsToday(nLastLaunch)
		if ( bIsToday )
		{
			return false
		}
		
		return true
	}
	
	var bPassCheck = CheckProcess()
	if (!bPassCheck)
	{
		CommonFun.log("[CheckLaunchCond] CheckProcess failed ")
		return false
	}
	
	var bPassCheck = CheckHistory()
	if (!bPassCheck)
	{
		CommonFun.log("[CheckLaunchCond] CheckHistory failed ")
		return false
	}

	return true
}


function LnchInTime.Exit(nExitCode)
{
	var tReport = {}
	tReport.EC = "lnch"
	tReport.EA = "exit"
	tReport.EL = nExitCode.toString()
	CommonFun.SendReport(tReport)
	
	var objCloudCount = new ClassCloudCount()
	objCloudCount.Decrease("LnchInTime enter")
}

function LnchInTime.SendStartReport()
{
	var tReport = {}
	tReport.EC = "lnch"
	tReport.EA = "start"
	CommonFun.SendReport(tReport)
}


function LnchInTime.SaveHistory(strKey)
{
	var strHistIniPath = CommonFun.GetHistoryPath()
	var nCurrentUTC = CommonFun.GetCurrentUTCTime()
	CommonFun.WriteINIFile(strHistIniPath, "launchldt", strKey, nCurrentUTC.toString())
}


function LnchInTime.Main()
{
	var objCloudCount = new ClassCloudCount()
	objCloudCount.Increase("LnchInTime enter")

	LnchInTime.SendStartReport()
	
	var bLnchSuccess = true
	var bPassCheck = LnchInTime.CheckLaunchCond()
	if (!bPassCheck)
	{
		CommonFun.log("CheckLaunchCond failed")
		LnchInTime.Exit(1)
		return false
	}
	
	var strExePath = LnchInTime.GetExePath()
	if (!CommonFun.QueryFileExists(strExePath))
	{
		LnchInTime.Exit(2)
		return false
	}
	var strCMD = "\""+strExePath+"\""+" -run"
	var WshShell =  new ActiveXObject("WScript.Shell")
	
	var bRet = WshShell.Run(strCMD)
	if (0 == bRet)
	{
		LnchInTime.SaveHistory("launch")
		LnchInTime.Exit(9)
	}
	else
	{
		LnchInTime.Exit(3)
	}	
}


LnchInTime.Main()