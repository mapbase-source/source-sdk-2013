import winreg
import vdf
import os
import xml.etree.ElementTree as ET

def create_vcxproj_user(vcxproj_name, debugger_command, debugger_args):
    # Get the current directory where the Python script is located
    script_dir = os.path.dirname(os.path.realpath(__file__))

    # Build the relative path to the game directory (one level up from the script)
    game_path = os.path.abspath(os.path.join(script_dir, '..', 'game', 'fracture_point'))

    # Modify the -game argument to use the dynamically determined game path
    updated_debugger_args = debugger_args.replace(
        'E:\\projects\\mapbase-extras\\sp\\game\\fracture_point', game_path
    )

    # Construct the path to the .vcxproj.user file
    vcxproj_user_path = os.path.join(script_dir, 'game', 'client', f'{vcxproj_name}.vcxproj.user')

    # Create the root XML element
    project = ET.Element('Project', {
        'ToolsVersion': 'Current',
        'xmlns': 'http://schemas.microsoft.com/developer/msbuild/2003'
    })

    # Function to add PropertyGroup for a given configuration
    def add_property_group(configuration):
        property_group = ET.SubElement(project, 'PropertyGroup', {
            'Condition': f"'$(Configuration)|$(Platform)'=='{configuration}|Win32'"
        })
        ET.SubElement(property_group, 'LocalDebuggerCommand').text = debugger_command
        ET.SubElement(property_group, 'LocalDebuggerCommandArguments').text = updated_debugger_args
        ET.SubElement(property_group, 'DebuggerFlavor').text = 'WindowsLocalDebugger'

    # Add both Debug and Release configurations
    add_property_group('Debug')
    add_property_group('Release')

    # Convert the tree to a string with the XML declaration
    xml_declaration = '<?xml version="1.0" encoding="utf-8"?>\n'
    xml_content = xml_declaration + ET.tostring(project, encoding='unicode')

    # Ensure the directory exists
    os.makedirs(os.path.dirname(vcxproj_user_path), exist_ok=True)

    # Write to the .vcxproj.user file
    with open(vcxproj_user_path, 'w', encoding='utf-8') as file:
        file.write(xml_content)

    print(f"Created {vcxproj_user_path} with dynamic -game path: {game_path}")

def get_steam_install_directory():
    try:
        # Open the registry key
        reg_key = winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam")
        
        # Retrieve the value of the Steam install path
        install_dir, _ = winreg.QueryValueEx(reg_key, "InstallPath")
        return install_dir
    except FileNotFoundError:
        print("Steam is not installed or the registry key is missing.")
        exit(0)
        return None

def find_game_install_path(vdf_path, target_appid):
    with open(vdf_path, 'r') as file:
        # Pass the file object directly to vdf.parse
        data = vdf.parse(file)
    
    # Loop through each library folder in the VDF file
    for key, library in data.get("libraryfolders", {}).items():
        path = library.get("path")
        apps = library.get("apps", {})
        
        # Check if the target AppID is in the current library's apps list
        if target_appid in apps:
            return path  # Return the library path where the game is installed
    
    return None  # Return None if the AppID is not found in any library

# Example usage
steam_install_path = fr"{get_steam_install_directory()}\steamapps\libraryfolders.vdf"
target_appid = "243730"

install_path = find_game_install_path(steam_install_path, target_appid)

if install_path:
    print(f"AppID {target_appid} is installed in: {install_path}")
    debugger_args = r'-console -toconsole -condebug -allowdebug -dev -w 1920 -h 1080 -noborder -window -debug -dev -game "E:\projects\mapbase-extras\sp\game\fracture_point"'
    debugger_command = fr"{install_path}\steamapps\common\Source SDK Base 2013 Singleplayer\hl2.exe"
    create_vcxproj_user("client_fp", debugger_command, debugger_args)
else:
    print(f"AppID {target_appid} is not found in any Steam library. Please install Source SDK 2013 Singleplayer")