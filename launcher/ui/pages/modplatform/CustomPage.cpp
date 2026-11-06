#include "CustomPage.h"
#include "ui_CustomPage.h"

#include <QTabBar>

#include "Application.h"
#include "Filter.h"
#include "Version.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "ui/dialogs/NewInstanceDialog.h"

CustomPage::CustomPage(NewInstanceDialog* dialog, QWidget* parent) : QWidget(parent), dialog(dialog), ui(new Ui::CustomPage)
{
    ui->setupUi(this);
    connect(ui->versionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedVersion);
    filterChanged();
    connect(ui->alphaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->betaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->snapshotFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->releaseFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->experimentsFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->refreshBtn, &QPushButton::clicked, this, &CustomPage::refresh);

    connect(ui->loaderVersionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedLoaderVersion);
    connect(ui->loaderBtnGroup, &QButtonGroup::buttonToggled, this, [this](QAbstractButton*, bool checked) {
        if (checked)
            loaderFilterChanged();
    });
    ui->goosePanel->hide();
    connect(ui->gooseVersion, &QComboBox::currentIndexChanged, this, [this] { suggestCurrent(); });
    connect(ui->loaderRefreshBtn, &QPushButton::clicked, this, &CustomPage::loaderRefresh);
    ui->loaderRefreshBtn->setEnabled(false);
}

void CustomPage::openedImpl()
{
    if (!initialized) {
        auto vlist = APPLICATION->metadataIndex()->get("net.minecraft");
        ui->versionList->initialize(vlist.get());
        initialized = true;
    } else {
        suggestCurrent();
    }
}

void CustomPage::refresh()
{
    ui->versionList->loadList();
}

void CustomPage::loaderRefresh()
{
    if (ui->noneFilter->isChecked() || ui->gooseFilter->isChecked())
        return;
    ui->loaderVersionList->loadList();
}

void CustomPage::filterChanged()
{
    QStringList out;
    if (ui->alphaFilter->isChecked())
        out << "(alpha)";
    if (ui->betaFilter->isChecked())
        out << "(beta)";
    if (ui->snapshotFilter->isChecked())
        out << "(snapshot)";
    if (ui->releaseFilter->isChecked())
        out << "(release)";
    if (ui->experimentsFilter->isChecked())
        out << "(experiment)";
    auto regexp = out.join('|');
    ui->versionList->setFilter(BaseVersionList::TypeRole, Filters::regexp(QRegularExpression(regexp)));
}

void CustomPage::loaderFilterChanged()
{
    const bool goose = ui->gooseFilter->isChecked();
    ui->loaderVersionList->setVisible(!goose);
    ui->goosePanel->setVisible(goose);
    ui->loaderRefreshBtn->setEnabled(!goose && !ui->noneFilter->isChecked());
    if (goose) {
        m_selectedLoader.clear();
        m_selectedLoaderVersion.reset();
        suggestCurrent();
        return;
    }
    QString minecraftVersion;
    if (m_selectedVersion) {
        minecraftVersion = m_selectedVersion->descriptor();
    } else {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        ui->loaderVersionList->setEmptyString(tr("No Minecraft version is selected."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
        return;
    }
    if (ui->noneFilter->isChecked()) {
        m_selectedLoader.clear();
        m_selectedLoaderVersion.reset();
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        ui->loaderVersionList->setEmptyString(tr("No mod loader is selected."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
        suggestCurrent();
        return;
    } else if (ui->neoForgeFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.neoforged";
    } else if (ui->forgeFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "net.minecraftforge";
    } else if (ui->fabricFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Fabric's dependencies
        if (Version(minecraftVersion) >= Version("1.14"))  // Fabric/Quilt supported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        else                                                                                   // Fabric/Quilt unsupported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        m_selectedLoader = "net.fabricmc.fabric-loader";
    } else if (ui->quiltFilter->isChecked()) {
        // FIXME: dirty hack because the launcher is unaware of Quilt's dependencies (same as Fabric)
        if (Version(minecraftVersion) >= Version("1.14"))  // Fabric/Quilt supported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "");
        else                                                                                   // Fabric/Quilt unsupported
            ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // clear list
        m_selectedLoader = "org.quiltmc.quilt-loader";
    } else if (ui->liteLoaderFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, minecraftVersion);
        m_selectedLoader = "com.mumfrey.liteloader";
    }

    auto vlist = APPLICATION->metadataIndex()->get(m_selectedLoader);
    ui->loaderVersionList->initialize(vlist.get());
    ui->loaderVersionList->selectRecommended();
    ui->loaderVersionList->setEmptyString(tr("No versions are currently available for Minecraft %1").arg(minecraftVersion));
    m_selectedLoaderVersion = ui->loaderVersionList->selectedVersion();
    suggestCurrent();
}

CustomPage::~CustomPage()
{
    delete ui;
}

bool CustomPage::shouldDisplay() const
{
    return true;
}

void CustomPage::retranslate()
{
    ui->retranslateUi(this);
}

BaseVersion::Ptr CustomPage::selectedVersion() const
{
    return m_selectedVersion;
}

BaseVersion::Ptr CustomPage::selectedLoaderVersion() const
{
    return m_selectedLoaderVersion;
}

QString CustomPage::selectedLoader() const
{
    return m_selectedLoader;
}

void CustomPage::suggestCurrent()
{
    if (!isOpened) {
        return;
    }

    if (ui->gooseFilter->isChecked()) {
        // A native engine is not a Java mod loader. Never create a Vanilla
        // instance under the Goose name while native builds are unavailable.
        dialog->setSuggestedPack(tr("Goose %1 (in development)").arg(ui->gooseVersion->currentText()), nullptr);
        return;
    }

    if (!m_selectedVersion) {
        dialog->setSuggestedPack();
        return;
    }

    // There isn't a selected version if the version list is empty
    if (ui->noneFilter->isChecked() || ui->loaderVersionList->selectedVersion() == nullptr)
        dialog->setSuggestedPack(m_selectedVersion->descriptor(), new VanillaCreationTask(m_selectedVersion));
    else {
        dialog->setSuggestedPack(m_selectedVersion->descriptor(),
                                 new VanillaCreationTask(m_selectedVersion, m_selectedLoader, m_selectedLoaderVersion));
    }
    dialog->setSuggestedIcon("default");
}

void CustomPage::setSelectedVersion(BaseVersion::Ptr version)
{
    m_selectedVersion = version;
    loaderFilterChanged();
    suggestCurrent();
}

void CustomPage::setSelectedLoaderVersion(BaseVersion::Ptr version)
{
    m_selectedLoaderVersion = version;
    suggestCurrent();
}
