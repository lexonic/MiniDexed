//
// uimain.cpp
//
// MiniDexed - Dexed FM synthesizer for bare metal Raspberry Pi
// Copyright (C) 2022  The MiniDexed Team
//
// Original author of this class:
//	B. Lex <@>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "uimain.h"
#include "minidexed.h"
#include "mididevice.h"
#include "userinterface.h"
#include "sysexfileloader.h"
#include "config.h"
#include <cmath>
#include <circle/logger.h>		// DEBUG (temporary)
#include <circle/sysconfig.h>
#include <assert.h>

LOGMODULE ("uimain");		// DEBUG (temporary)


using namespace std;

const CUIMain::TMainPage CUIMain::s_MainScreens[] =
{
	//{ScreenHandler, EventFunctionsTable}
	{ViewBankVoice, s_BankVoiceFunctions},
	{ViewPerformance, s_PerformanceFunctions},
	{ViewTGGroupMute, s_GroupsMuteFunctions},
	{0}
};

const CUIMain::TMainItem CUIMain::s_BankVoiceFunctions[] =
{
	//{shortPressFunc, encoderFunc, pressAndTurnFunc}
	{NextBankVoiceMainItem, ChangeTG, ChangeMasterVol},		// TG selected
	{NextBankVoiceMainItem, ChangeBank, ChangeMasterVol},	// bank selected
	{NextBankVoiceMainItem, ChangeVoice, ChangeMasterVol},	// voice selected
	{0}
};

const CUIMain::TMainItem CUIMain::s_PerformanceFunctions[] =
{
	//{shortPressFunc, encoderFunc, pressAndTurnFunc}
	{LoadPerf, SelectPerf, ChangeMasterVol},		// performance
	{0}
};

const CUIMain::TMainItem CUIMain::s_GroupsMuteFunctions[] =
{
	//{shortPressFunc, encoderFunc, pressAndTurnFunc}
	{MuteUnmuteTG, SelectTG, EditGroup},	// mute & group
	{0}
};


// CUIMain constructor
CUIMain::CUIMain (CUserInterface *pUI, CMiniDexed *pMiniDexed)
:	m_pUI (pUI),
	m_pMiniDexed (pMiniDexed),
	m_nCurrentMainScreen (0),
	//m_nCurrentMainItem (0),
	m_nCurrentSelection (0),
	m_nCurrentTG (0),
	m_nSelectedTG (0)
{
}


// EventHandler
void CUIMain::EventHandler (TMainEvent Event)
{
	switch (Event)
	{
	case MainEventNextScreen:
		m_nCurrentMainScreen++;
		m_nCurrentSelection = 0;
		if (s_MainScreens[m_nCurrentMainScreen].ScreenHandler == 0)
		{
			m_nCurrentMainScreen = 0;
		}
		// always start with actual loaded performance when entering Performance menu
		// TODO: should the displayed performance also be updated on performance change by MIDI?
		m_nSelectedPerformanceID = m_pMiniDexed->GetActualPerformanceID();
		break;

	case MainEventSelect:
		s_MainScreens[m_nCurrentMainScreen].FuncItem[m_nCurrentSelection].ShortPressHandler(this, Event);
		// TODO: for my learning experience
		// following compiled correctly, but had no index for FuncItem -> used always index 0
		//s_MainScreens[m_nCurrentMainScreen].FuncItem->ShortPressHandler(this, Event);
		break;

	case MainEventStepDown:
	case MainEventStepUp:
		s_MainScreens[m_nCurrentMainScreen].FuncItem[m_nCurrentSelection].EncoderHandler(this, Event);
		break;

	case MainEventPressAndStepDown:
	case MainEventPressAndStepUp:
		s_MainScreens[m_nCurrentMainScreen].FuncItem[m_nCurrentSelection].PressAndTurnHandler(this, Event);

		// ChangeMasterVol() takes care about its own screen. Don't run the regular display
		TMainHandler *pPressAndTurnFunc;
		pPressAndTurnFunc = s_MainScreens[m_nCurrentMainScreen].FuncItem[m_nCurrentSelection].PressAndTurnHandler;
		if (pPressAndTurnFunc == ChangeMasterVol)
		{
			return;
		}
		break;

	default:
		break;
	}

	// display screen
	(*s_MainScreens[m_nCurrentMainScreen].ScreenHandler)(this);
}


// ChangeBank
void CUIMain::ChangeBank (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nTG = pUIMain->m_nCurrentTG;
	int nTGGroup = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, nTG);

	int nValue = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);

	switch (Event)
	{
	case MainEventStepDown:
		nValue = pUIMain->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nValue);
		break;

	case MainEventStepUp:
		nValue = pUIMain->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nValue);
		break;

	default:
		return;
	}

	if (nTGGroup == 0)
	{
		// change bank of this TG only
		pUIMain->m_pMiniDexed->SetTGParameter(
			CMiniDexed::TGParameterVoiceBank, nValue, nTG);
	}
	else
	{
		// change banks of all TG's in same Group
		for (unsigned n = 0; n < CConfig::ToneGenerators; n++)
		{
			if (pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, n) == nTGGroup)
			{
				pUIMain->m_pMiniDexed->SetTGParameter(
					CMiniDexed::TGParameterVoiceBank, nValue, n);
			}
		}
	}
}


// ChangeVoice
void CUIMain::ChangeVoice (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nTG = pUIMain->m_nCurrentTG;
	int nTGGroup = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, nTG);
	int nVB = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);

	int nValue = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterProgram, nTG);

	bool bUpdateBank = false;

	switch (Event)
	{
	case MainEventStepDown:
		if (--nValue < 0)
		{
			// Switch down a voice bank and set to the last voice
			bUpdateBank = true;
			nValue = CSysExFileLoader::VoicesPerBank-1;
			nVB = pUIMain->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankDown(nVB);
		}
		break;

	case MainEventStepUp:
		if (++nValue > (int) CSysExFileLoader::VoicesPerBank-1)
		{
			// Switch up a voice bank and reset to voice 0
			bUpdateBank = true;
			nValue = 0;
			nVB = pUIMain->m_pMiniDexed->GetSysExFileLoader ()->GetNextBankUp(nVB);
		}
		break;

	default:
		return;
	}

	if (nTGGroup == 0)
	{
		// change voice of this TG only
		if (bUpdateBank)
		{
			pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterVoiceBank, nVB, nTG);
		}

		pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterProgram, nValue, nTG);
	}
	else
	{
		// change banks of all TG's in same Group
		for (unsigned n = 0; n < CConfig::ToneGenerators; n++)
		{
			if (pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, n) == nTGGroup)
			{
				if (bUpdateBank)
				{
					pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterVoiceBank, nVB, n);
				}

				pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterProgram, nValue, n);
			}
		}
	}
}


// ChangeTG
void CUIMain::ChangeTG (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nValue = pUIMain->m_nCurrentTG;

	switch (Event)
	{
	case MainEventStepDown:
		if (nValue > 0)
		{
			nValue--;
		}
		break;

	case MainEventStepUp:
		if (++nValue >= CConfig::ToneGenerators)
		{
			nValue = CConfig::ToneGenerators - 1;
		}
		break;

	default:
		return;
	}

	pUIMain->m_nCurrentTG = nValue;
}


// MuteUnmuteTG
void CUIMain::MuteUnmuteTG (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nTG = pUIMain->m_nSelectedTG;
	unsigned nTGMute = (pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGEnable, nTG)) ? 0 : 1;

	// toggle TGParameterTGEnabled
	pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterTGEnable, nTGMute, nTG);
}


// SelectTG
void CUIMain::SelectTG (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nValue = pUIMain->m_nSelectedTG;

	switch (Event)
	{
	case MainEventStepDown:
		if (nValue > 0)
		{
			nValue--;
		}
		break;

	case MainEventStepUp:
		if (++nValue >= CConfig::ToneGenerators)
		{
			nValue = CConfig::ToneGenerators - 1;
		}
		break;

	default:
		return;
	}

	pUIMain->m_nSelectedTG = nValue;
}


// EditGrouping
void CUIMain::EditGroup (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nTG = pUIMain->m_nSelectedTG;
	unsigned nGroupID = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, nTG);
	switch (Event)
	{
	case MainEventPressAndStepDown:
		if (nGroupID > 0)
		{
			nGroupID--;
		}
		break;

	case MainEventPressAndStepUp:
		if (nGroupID < 4)
		{
			nGroupID++;
		}
		break;

	default:
		return;
	}

	pUIMain->m_pMiniDexed->SetTGParameter(CMiniDexed::TGParameterTGGrouping, nGroupID, nTG);
}


// LoadPerformance
void CUIMain::LoadPerf (CUIMain *pUIMain, TMainEvent Event)
{
	if (pUIMain->m_pMiniDexed->GetPerformanceSelectToLoad())
	{
		pUIMain->m_pMiniDexed->SetNewPerformance(pUIMain->m_nSelectedPerformanceID);
	}
}


// SelectPerformance
void CUIMain::SelectPerf (CUIMain *pUIMain, TMainEvent Event)
{
	bool bPerformanceSelectToLoad = pUIMain->m_pMiniDexed->GetPerformanceSelectToLoad();
	unsigned nValue = pUIMain->m_nSelectedPerformanceID;

	switch (Event)
	{
	case MainEventStepDown:
		if (nValue > 0)
		{
			--nValue;
		}
		pUIMain->m_nSelectedPerformanceID = nValue;
		// auto-load performance on selection
		if (!bPerformanceSelectToLoad)
		{
			pUIMain->m_pMiniDexed->SetNewPerformance(nValue);
		}
		break;

	case MainEventStepUp:
		if (++nValue > (unsigned) pUIMain->m_pMiniDexed->GetLastPerformance()-1)
		{
			nValue = pUIMain->m_pMiniDexed->GetLastPerformance()-1;
		}
		pUIMain->m_nSelectedPerformanceID = nValue;
		// auto-load performance on selection
		if (!bPerformanceSelectToLoad)
		{
			pUIMain->m_pMiniDexed->SetNewPerformance(nValue);
		}
		break;

	default:
		return;
	}
}


// ChangeMasterVolume
void CUIMain::ChangeMasterVol (CUIMain *pUIMain, TMainEvent Event)
{
	int nValue = (int)roundf(pUIMain->m_pMiniDexed->getMasterVolume() * 100);

	switch (Event)
	{
	case MainEventPressAndStepDown:
		nValue -= 5;
		if (nValue < 0)
		{
			nValue = 0;
		}
		break;

	case MainEventPressAndStepUp:
		nValue += 5;
		if (nValue > 100)
		{
			nValue = 100;
		}
		break;

	default:
		return;
	}

	//float32_t nMasterVol = nValue;
	//pUIMain->m_pMiniDexed->setMasterVolume(nMasterVol / 100);
	pUIMain->m_pMiniDexed->setMasterVolume((float32_t)nValue / 100);

	pUIMain->ViewMasterVol(pUIMain);

	// TODO: it this creating a new timer on every tick of the encoder???
	//CTimer::Get()->StartKernelTimer(MSEC2HZ (1500), pUIMain->TimerHandler, 0, pUIMain);
	if (pUIMain->m_TimerHandle)
	{
		// before we start a new KernelTimer, we have to cancel the running one
		// or it will trigger unexpectedly and exit the MasterVolume-Screen.
		// It seems there is no way to extend the time of the running KernelTimer.
		CTimer::Get()->CancelKernelTimer(pUIMain->m_TimerHandle);		// TODO: what if timer already elapsed?
	}
	pUIMain->m_TimerHandle = CTimer::Get()->StartKernelTimer(MSEC2HZ (1500), pUIMain->TimerHandler, 0, pUIMain);
}


// NextBankVoiceMainItem
void CUIMain::NextBankVoiceMainItem (CUIMain *pUIMain, TMainEvent Event)
{
	pUIMain->m_nCurrentSelection++;
	if (s_MainScreens[pUIMain->m_nCurrentMainScreen].FuncItem[pUIMain->m_nCurrentSelection].ShortPressHandler == 0)
		{
			pUIMain->m_nCurrentSelection = 0;
		}
}


// ViewBankVoice
void CUIMain::ViewBankVoice (CUIMain *pUIMain)
{
	unsigned nTG = pUIMain->m_nCurrentTG;

	string Name;
	string NamePadded;
	string ValuePadded;

	// prepare selection brackets
	string BankSelectedOpenBracket = " ";
	string BankSelectedCloseBracket = " ";
	string VoiceSelectedOpenBracket = " ";
	string VoiceSelectedCloseBracket = " ";
	TMainHandler *pEncoderFunc;
	pEncoderFunc = s_MainScreens[pUIMain->m_nCurrentMainScreen].FuncItem[pUIMain->m_nCurrentSelection].EncoderHandler;

	if (pEncoderFunc == ChangeBank)
	{
		BankSelectedOpenBracket = "[";
		BankSelectedCloseBracket = "]";
	}
	if (pEncoderFunc == ChangeVoice)
	{
		VoiceSelectedOpenBracket = "[";
		VoiceSelectedCloseBracket = "]";
	}


	// -- Bank --
	int nBankValue = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterVoiceBank, nTG);

	// padding BankValue with "0"s to 3 char
	ValuePadded = pUIMain->PadString(to_string(nBankValue+1).c_str(), 3, '0', true);

	// padding BankName to 10 char
	Name = pUIMain->m_pMiniDexed->GetSysExFileLoader()->GetBankName(nBankValue);
	NamePadded = pUIMain->PadString(Name.c_str(), 10, ' ', false);

	string Bank = ValuePadded + BankSelectedOpenBracket
				+ NamePadded + BankSelectedCloseBracket;

	// -- Voice --
	int nVoiceValue = pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterProgram, nTG);

	// padding VoiceValue to 3 char (format: " 00")
	ValuePadded = " ";
	ValuePadded += pUIMain->PadString(to_string(nVoiceValue+1).c_str(), 2, '0', true);

	// padding VoiceName to 10 char
	Name = pUIMain->m_pMiniDexed->GetVoiceName(nTG);
	NamePadded = pUIMain->PadString(Name.c_str(), 10, ' ', false);

	string Voice = ValuePadded + VoiceSelectedOpenBracket 
				 + NamePadded + VoiceSelectedCloseBracket;

	// TG
	string TG = to_string(nTG+1);

	// MIDI
	string Midi = " ";	// DUMMY for now.  TODO!

	pUIMain->m_pUI->DisplayWriteMain(Bank.c_str(), TG.c_str(), Voice.c_str(), Midi.c_str());
}


// ViewPerformance
void CUIMain::ViewPerformance (CUIMain *pUIMain)
{
	unsigned nValue = pUIMain->m_nSelectedPerformanceID;
	const char empty[] = "";

	string ValuePadded = "PERF ";
	// padding PerformanceValue with "0"s to 4 char
	ValuePadded += pUIMain->PadString(to_string(nValue).c_str(), 4, '0', true);

	string Name = pUIMain->m_pMiniDexed->GetPerformanceName(nValue);

	// indicator for "Performance Loaded"
	string nPSelected = "";
	if(nValue == pUIMain->m_pMiniDexed->GetActualPerformanceID())
		{
			nPSelected= "(L)";
		}

	pUIMain->m_pUI->DisplayWriteMain(ValuePadded.c_str(), nPSelected.c_str(), Name.c_str(), empty);
}


// ViewTGGroupMute
void CUIMain::ViewTGGroupMute (CUIMain *pUIMain)
{
	// TODO
	//const char line1L[] = "TG:Grp+Mute";
	//const char line2L[] = "to do...";
	//const char lineXR[] = "";
	const char empty[] = "";
	string Groups = "Group ";
	string TGs =	"TG    ";

	for (unsigned n = 0; n < CConfig::ToneGenerators; n++)
	{
		string Group(1, pUIMain->m_GroupText[pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGGrouping, n)]);

		Groups += Group;
		TGs += (pUIMain->m_pMiniDexed->GetTGParameter(CMiniDexed::TGParameterTGEnable, n)) ? to_string(n+1) : "-";
	}

	// set cursor to selected group
	// \E[2;%dH	Cursor move to row 2 and column %d (starting at 1)
	// \E[?25h	Normal cursor visible
	// \E[?25l	Cursor invisible
	
	string escCursor="\E[?25h\E[2;"; // this is to locate cursor
	escCursor += to_string(pUIMain->m_nSelectedTG + 7);
	escCursor += "H";

	pUIMain->m_pUI->DisplayWriteMain(Groups.c_str(), empty, TGs.c_str(), escCursor.c_str());
}


// ViewMasterVolume
void CUIMain::ViewMasterVol (CUIMain *pUIMain)
{
	const char textMasterVolume[] = "Master Volume";
	const char empty[] = "";

	unsigned nValue = (unsigned)roundf(pUIMain->m_pMiniDexed->getMasterVolume() * 100);
	string VolumeBar = "[" + pUIMain->ToVolume(nValue) + "]";

	pUIMain->m_pUI->DisplayWriteMain(textMasterVolume, empty, VolumeBar.c_str(), to_string(nValue).c_str());
}


// PadString
string CUIMain::PadString (const char *pSource, int nLength, char PadChar /*=' '*/, bool bPrependPadding /*=false*/)
{
	string Padded = "";
	string Padding(nLength - strlen(pSource), PadChar);

	if (!bPrependPadding) 
	{
		Padded = pSource;
	}

	Padded += Padding;

	if (bPrependPadding) 
	{
		Padded += pSource;
	}
	return Padded;
}


// ToVolume
string CUIMain::ToVolume (int nValue)
{
	static const size_t MaxChars = CConfig::LCDColumns-6;
	char VolumeBar[MaxChars+1];
	memset (VolumeBar, 0xFF, sizeof VolumeBar);	// 0xFF is the block character
	VolumeBar[nValue * MaxChars / 100] = '\0';
	// fill the remaining of the bar with SPACE
	string FullVolumeBar = PadString(VolumeBar, MaxChars, ' ', false);

	return FullVolumeBar;
}


// TimerHandler
void CUIMain::TimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMain *pThis = static_cast<CUIMain *> (pContext);
	assert (pThis);

	pThis->EventHandler (MainEventUpdate);
}


/*
void CUIMain::MainHandler (CUIMain *pUIMain, TMainEvent Event)
{
	switch (Event)
	{
	case MainEventUpdate:
		break;

	case MainEventSelect:				// push menu
		assert (pUIMain->m_nCurrentMainDepth < MaxMainDepth);
		pUIMain->m_MainStackParent[pUIMain->m_nCurrentMainDepth] = pUIMain->m_pParentMain;
		pUIMain->m_MainStackMain[pUIMain->m_nCurrentMainDepth] = pUIMain->m_pCurrentMain;
		pUIMain->m_nMainStackItem[pUIMain->m_nCurrentMainDepth]
			= pUIMain->m_nCurrentMainItem;
		pUIMain->m_nMainStackSelection[pUIMain->m_nCurrentMainDepth]
			= pUIMain->m_nCurrentSelection;
		pUIMain->m_nMainStackParameter[pUIMain->m_nCurrentMainDepth]
			= pUIMain->m_nCurrentParameter;
		pUIMain->m_nCurrentMainDepth++;

		pUIMain->m_pParentMain = pUIMain->m_pCurrentMain;
		pUIMain->m_nCurrentParameter =
			pUIMain->m_pCurrentMain[pUIMain->m_nCurrentSelection].Parameter;
		pUIMain->m_pCurrentMain =
			pUIMain->m_pCurrentMain[pUIMain->m_nCurrentSelection].MainItem;
		pUIMain->m_nCurrentMainItem = pUIMain->m_nCurrentSelection;
		pUIMain->m_nCurrentSelection = 0;
		break;

	case MainEventStepDown:
		if (pUIMain->m_nCurrentSelection > 0)
		{
			pUIMain->m_nCurrentSelection--;
		}
		break;

	case MainEventStepUp:
		++pUIMain->m_nCurrentSelection;
		if (!pUIMain->m_pCurrentMain[pUIMain->m_nCurrentSelection].Name)  // more entries?
		{
			pUIMain->m_nCurrentSelection--;
		}
		break;

	default:
		return;
	}

	if (pUIMain->m_pCurrentMain)				// if this is another menu?
	{
		pUIMain->m_pUI->DisplayWrite (
			pUIMain->m_pParentMain[pUIMain->m_nCurrentMainItem].Name,
			"",
			pUIMain->m_pCurrentMain[pUIMain->m_nCurrentSelection].Name,
			pUIMain->m_nCurrentSelection > 0,
			!!pUIMain->m_pCurrentMain[pUIMain->m_nCurrentSelection+1].Name);
	}
	else
	{
		pUIMain->EventHandler (MainEventUpdate);	// no, update parameter display
	}
}


void CUIMain::EditVoiceParameter (CUIMain *pUIMain, TMainEvent Event)
{
	unsigned nTG = pUIMain->m_nMainStackParameter[pUIMain->m_nCurrentMainDepth-2];

	unsigned nParam = pUIMain->m_nCurrentParameter;
	const TParameter &rParam = s_VoiceParameter[nParam];

	int nValue = pUIMain->m_pMiniDexed->GetVoiceParameter (nParam, CMiniDexed::NoOP, nTG);

	switch (Event)
	{
	case MainEventUpdate:
		break;

	case MainEventStepDown:
		nValue -= rParam.Increment;
		if (nValue < rParam.Minimum)
		{
			nValue = rParam.Minimum;
		}
		pUIMain->m_pMiniDexed->SetVoiceParameter (nParam, nValue, CMiniDexed::NoOP, nTG);
		break;

	case MainEventStepUp:
		nValue += rParam.Increment;
		if (nValue > rParam.Maximum)
		{
			nValue = rParam.Maximum;
		}
		pUIMain->m_pMiniDexed->SetVoiceParameter (nParam, nValue, CMiniDexed::NoOP, nTG);
		break;

	case MainEventPressAndStepDown:
	case MainEventPressAndStepUp:
		pUIMain->TGShortcutHandler (Event);
		return;

	default:
		return;
	}

	string TG ("TG");
	TG += to_string (nTG+1);

	string Value = GetVoiceValueString (nParam, nValue);

	pUIMain->m_pUI->DisplayWrite (TG.c_str (),
					  pUIMain->m_pParentMain[pUIMain->m_nCurrentMainItem].Name,
					  Value.c_str (),
					  nValue > rParam.Minimum, nValue < rParam.Maximum);
}


string CUIMain::GetTGValueString (unsigned nTGParameter, int nValue)
{
	string Result;

	assert (nTGParameter < sizeof CUIMain::s_TGParameter / sizeof CUIMain::s_TGParameter[0]);

	CUIMain::TToString *pToString = CUIMain::s_TGParameter[nTGParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMain::GetVoiceValueString (unsigned nVoiceParameter, int nValue)
{
	string Result;

	assert (nVoiceParameter < sizeof CUIMain::s_VoiceParameter / sizeof CUIMain::s_VoiceParameter[0]);

	CUIMain::TToString *pToString = CUIMain::s_VoiceParameter[nVoiceParameter].ToString;
	if (pToString)
	{
		Result = (*pToString) (nValue);
	}
	else
	{
		Result = to_string (nValue);
	}

	return Result;
}

string CUIMain::ToVolume (int nValue)
{
	static const size_t MaxChars = CConfig::LCDColumns-2;
	char VolumeBar[MaxChars+1];
	memset (VolumeBar, 0xFF, sizeof VolumeBar);	// 0xFF is the block character
	VolumeBar[nValue * MaxChars / 127] = '\0';

	return VolumeBar;
}


void CUIMain::TimerHandlerNoBack (TKernelTimerHandle hTimer, void *pParam, void *pContext)
{
	CUIMain *pThis = static_cast<CUIMain *> (pContext);
	assert (pThis);
	
	pThis->m_bSplashShow = false;
	
	pThis->EventHandler (MainEventUpdate);
}
*/
