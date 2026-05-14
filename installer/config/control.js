function Controller()
{
    // 安装前检查
    installer.installationFinished.connect(this, Controller.prototype.onInstallationFinished);
    installer.uninstallationFinished.connect(this, Controller.prototype.onUninstallationFinished);
}

Controller.prototype.IntroductionPageCallback = function()
{
    // 欢迎页面自定义
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    // 目标目录页面自定义
    // 可以在这里设置默认安装路径
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    // 组件选择页面自定义
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.onInstallationFinished = function()
{
    // 安装完成后执行
    if (installer.isInstaller()) {
        // 可以在这里运行后安装脚本
        console.log("安装完成！");
    }
}

Controller.prototype.onUninstallationFinished = function()
{
    // 卸载完成后执行
    console.log("卸载完成！");
}
