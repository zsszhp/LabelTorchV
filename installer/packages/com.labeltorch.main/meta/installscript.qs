function Component()
{
    // 构造函数：保持标准向导页面可见
    installer.setDefaultPageVisible(QInstaller.Introduction, true);
    installer.setDefaultPageVisible(QInstaller.TargetDirectory, true);
    installer.setDefaultPageVisible(QInstaller.ComponentSelection, true);
    installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, true);
    installer.setDefaultPageVisible(QInstaller.PerformInstallation, true);
    installer.setDefaultPageVisible(QInstaller.InstallationFinished, true);
}

Component.prototype.createOperations = function()
{
    // 创建默认操作（提取数据）。卸载策略：RemoveTargetDir=true 只移除安装目录，
    // 不触碰用户 AppData 中的项目数据库与数据集，避免误删用户数据。
    component.createOperations();

    // Windows：桌面 / 开始菜单快捷方式指向主程序
    if (systemInfo.productType === "windows") {
        component.addOperation("CreateShortcut",
            "@TargetDir@/LabelTorchV.exe",
            "@DesktopDir@/标炬.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/LabelTorchV.exe",
            "description=标炬工业缺陷检测软件");

        component.addOperation("CreateShortcut",
            "@TargetDir@/LabelTorchV.exe",
            "@StartMenuDir@/标炬/标炬.lnk",
            "workingDirectory=@TargetDir@",
            "iconPath=@TargetDir@/LabelTorchV.exe",
            "description=标炬工业缺陷检测软件");
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
            "@TargetDir@/LabelTorchV",
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
