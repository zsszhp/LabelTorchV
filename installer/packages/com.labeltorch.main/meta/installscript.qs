function Component()
{
    // 构造函数
    installer.setDefaultPageVisible(QInstaller.Introduction, true);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, true);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, true);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, true);
    installer.setDefaultPageVisible(QInstaller.PerformInstallation, true);
    installer.setDefaultPageVisible(QInstaller.InstallationFinished, true);
}

Component.prototype.createOperations = function()
{
    // 创建默认操作（提取数据）
    component.createOperations();

    // 添加桌面快捷方式（Windows）
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/labeltorch.exe",
            "@DesktopDir@/标炬.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/labeltorch.exe",
            "description=标炬工业缺陷检测软件");

        // 添加开始菜单快捷方式
        component.addOperation("CreateShortcut",
            "@TargetDir@/labeltorch.exe",
            "@StartMenuDir@/标炬/标炬.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/labeltorch.exe",
            "description=标炬工业缺陷检测软件");

        // 创建启动脚本
        component.addOperation("CreateShortcut",
            "@TargetDir@/labeltorch.exe",
            "@TargetDir@/启动标炬.bat",
            "workingDirectory=@TargetDir@");
    }

    // macOS
    if (systemInfo.productType === "osx") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/标炬.app",
            "@HomeDir@/Desktop/标炬",
            "workingDirectory=@TargetDir@");
    }

    // Linux
    if (systemInfo.productType === "linux") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/labeltorch",
            "@HomeDir@/Desktop/标炬",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/labeltorch.svg");

        // 设置执行权限
        component.addOperation("Execute",
            "@TargetDir@/启动标炬.sh",
            "chmod", "+x", "@TargetDir@/启动标炬.sh");
    }
}

Component.prototype.isDefault = function()
{
    return true;
}
