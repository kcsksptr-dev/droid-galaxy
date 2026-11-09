"""Editor helper: move the level viewport camera and take a HighResShot.
Usage (editor open): python3 Tools/ue_remote.py -c "import sys; sys.argv=['', 'x,y,z', 'pitch,yaw', 'name']; exec(open('Tools/ue_shot.py').read())"
Screenshots land in Saved/Screenshots/LinuxEditor/ (named HighresScreenshotNNNNN.png).
"""
import sys
import unreal

loc = [float(v) for v in sys.argv[1].split(",")]
pitch, yaw = [float(v) for v in sys.argv[2].split(",")]
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ues.set_level_viewport_camera_info(unreal.Vector(*loc), unreal.Rotator(roll=0, pitch=pitch, yaw=yaw))
unreal.SystemLibrary.execute_console_command(ues.get_editor_world(), "HighResShot 1600x900")
print("shot", sys.argv[3] if len(sys.argv) > 3 else "")
