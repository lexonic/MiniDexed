//
// uimain.h
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
#ifndef _uimain_h
#define _uimain_h

#include <string>
#include <circle/timer.h>

class CMiniDexed;
class CUserInterface;


class CUIMain
{
public:
	enum TMainEvent
	{
		MainEventUpdate,
		MainEventNextScreen,
		MainEventSelect,
		MainEventStepDown,
		MainEventStepUp,
		MainEventPressAndStepDown,
		MainEventPressAndStepUp,
		MainEventVolumeDown,		// use for dedicated Volume encoder
		MainEventVolumeUp,			// use for dedicated Volume encoder
		MainEventUnknown
	};

public:
	CUIMain (CUserInterface *pUI, CMiniDexed *pMiniDexed);

	void EventHandler (TMainEvent Event);
	
private:
	typedef void TMainHandler (CUIMain *pUIMain, TMainEvent Event);
	typedef void TMainScreen (CUIMain *pUIMain);

	struct TMainItem
	{
		TMainHandler *ShortPressHandler;
		TMainHandler *EncoderHandler;
		TMainHandler *PressAndTurnHandler;
	};

	struct TMainPage
	{
		TMainScreen *ScreenHandler;
		const TMainItem *FuncItem;
	};

	typedef std::string TToString (int nValue);

	struct TParameter
	{
		int Minimum;
		int Maximum;
		int Increment;
		TToString *ToString;
	};

private:
	static void MainHandler (CUIMain *pUIMain, TMainEvent Event);
	static void NextBankVoiceMainItem (CUIMain *pUIMain, TMainEvent Event);
	static void ChangeMasterVol (CUIMain *pUIMain, TMainEvent Event);
	static void ChangeBank (CUIMain *pUIMain, TMainEvent Event);
	static void ChangeVoice (CUIMain *pUIMain, TMainEvent Event);
	static void ChangeTG (CUIMain *pUIMain, TMainEvent Event);
	static void LoadPerf (CUIMain *pUIMain, TMainEvent Event);
	static void SelectPerf (CUIMain *pUIMain, TMainEvent Event);
	static void MuteUnmuteTG (CUIMain *pUIMain, TMainEvent Event);
	static void SelectTG (CUIMain *pUIMain, TMainEvent Event);
	static void EditGroup (CUIMain *pUIMain, TMainEvent Event);

	static void ViewBankVoice (CUIMain *pUIMain);
	static void ViewPerformance (CUIMain *pUIMain);
	static void ViewTGGroupMute (CUIMain *pUIMain);
	static void ViewMasterVol (CUIMain *pUIMain);

	static std::string ToVolume (int nValue);
	//static std::string ToMIDIChannel (int nValue);
	static std::string PadString (const char *pSource, int nLength, char PadChar = ' ', bool bPrependPadding = true);
	
	static void TimerHandler (TKernelTimerHandle hTimer, void *pParam, void *pContext);

private:
	CUserInterface *m_pUI;
	CMiniDexed *m_pMiniDexed;

	unsigned m_nCurrentMainScreen;	// index of active item from s_MainScreens[]
	//unsigned m_nCurrentMainItem;
	unsigned m_nCurrentSelection;	 // index of active item from a TMainItem table
	unsigned m_nCurrentTG;			// TG selected in BankVoice Screen
	unsigned m_nSelectedTG;			// TG selected in Group/Mute Screen

	static const TMainPage s_MainScreens[];
	static const TMainItem s_BankVoiceFunctions[];
	static const TMainItem s_PerformanceFunctions[];
	static const TMainItem s_GroupsMuteFunctions[];
	
	unsigned m_nSelectedPerformanceID=0;

	std::string m_GroupText=".ABCD";
	TKernelTimerHandle m_TimerHandle=0;
};

#endif
