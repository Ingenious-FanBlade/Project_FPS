using UnrealBuildTool;

public class Project_FPSServerTarget : TargetRules
{
    public Project_FPSServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("Project_FPS");
    }
}
