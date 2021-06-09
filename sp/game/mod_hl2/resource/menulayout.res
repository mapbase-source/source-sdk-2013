//-----------------------------------------------------------------------------
// Menu buttons
//-----------------------------------------------------------------------------
"MenuLayout"
{
    "#NovelUI_ResumeGame"
    {
        "command"   "ResumeGame"
        "in_game"   "1"
        "in_menu"   "0" 
    }

    "#NovelUI_Disconnect"
    {
        "command"   "Disconnect"
        "in_game"   "1"
        "in_menu"   "0"
    }
    
    "blank"
    {
        "blank"     "1"
        "in_game"   "1"
        "in_menu"   "0"
    }
    
    "#NovelUI_NewGame"
    {
        "command"   "OpenNewGameDialog"
    }

    "#NovelUI_LoadGame"
    {
        "command"   "OpenLoadGameDialog"
    }
    
    "#NovelUI_SaveGame"
    {
        "command"   "OpenSaveGameDialog"
        "in_game"   "1"
        "in_menu"   "0"
    }
    
    "blank"
    {
        "blank"     "1"
    }
    
    "#NovelUI_Options"
    {
        "command"   "OpenOptionsDialog"
    }
    
    "#NovelUI_Quit"
    {
        "command"   "Quit"
    }
}