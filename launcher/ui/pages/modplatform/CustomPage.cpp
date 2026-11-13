#include "CustomPage.h"
#include "ui_CustomPage.h"

#include <QTabBar>
#include <QLabel>

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
    m_rustFilter = new QCheckBox(tr("Rust"), this);
    m_rustFilter->setObjectName("rustFilter");
    m_rustFilter->setToolTip(tr("Rust 26.3 is in beta and does not support Java mod loaders."));
    ui->verticalLayout->insertWidget(6, m_rustFilter);
    m_rustVersion = new QLabel(tr("Rust — 26.3 Beta"), this);
    m_rustVersion->setObjectName("rustVersion");
    m_rustVersion->setAlignment(Qt::AlignCenter);
    m_rustVersion->setWordWrap(true);
    ui->minecraftLayout->insertWidget(0, m_rustVersion, 1);
    m_rustVersion->hide();
    connect(m_rustFilter, &QCheckBox::toggled, this, [this](bool rust) {
        ui->versionList->setVisible(!rust);
        m_rustVersion->setVisible(rust);
        for (auto* filter : {ui->releaseFilter, ui->snapshotFilter, ui->betaFilter, ui->alphaFilter, ui->experimentsFilter})
            filter->setEnabled(!rust);
        for (auto* loader : {ui->neoForgeFilter, ui->forgeFilter, ui->fabricFilter, ui->quiltFilter, ui->liteLoaderFilter})
            loader->setEnabled(!rust);
        ui->noneFilter->setChecked(true);
        ui->loaderVersionList->setEnabled(!rust);
        ui->loaderRefreshBtn->setEnabled(!rust);
        ui->refreshBtn->setEnabled(!rust);
        m_selectedLoader.clear();
        m_selectedLoaderVersion.reset();
        suggestCurrent();
        loaderFilterChanged();
    });
    connect(ui->versionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedVersion);
    filterChanged();
    connect(ui->alphaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->betaFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->snapshotFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->releaseFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->experimentsFilter, &QCheckBox::stateChanged, this, &CustomPage::filterChanged);
    connect(ui->refreshBtn, &QPushButton::clicked, this, &CustomPage::refresh);

    connect(ui->loaderVersionList, &VersionSelectWidget::selectedVersionChanged, this, &CustomPage::setSelectedLoaderVersion);
    connect(ui->noneFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->forgeFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->fabricFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->quiltFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->liteLoaderFilter, &QRadioButton::toggled, this, &CustomPage::loaderFilterChanged);
    connect(ui->loaderRefreshBtn, &QPushButton::clicked, this, &CustomPage::loaderRefresh);
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
    if (ui->noneFilter->isChecked())
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
    if (m_rustFilter->isChecked()) {
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");
        ui->loaderVersionList->setEmptyString(tr("Rust 26.3 is in beta and does not support Java mod loaders."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
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
        ui->loaderVersionList->setExactFilter(BaseVersionList::ParentVersionRole, "AAA");  // empty list
        ui->loaderVersionList->setEmptyString(tr("No mod loader is selected."));
        ui->loaderVersionList->setEmptyMode(VersionListView::String);
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
    m_rustFilter->setText(tr("Rust"));
    m_rustVersion->setText(tr("Rust — 26.3 Beta"));
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

    if (m_rustFilter->isChecked()) {
        auto* task = new VanillaCreationTask(QStringLiteral("26.3"));
        task->setNativeRust(true);
        dialog->setSuggestedPack(tr("Rust — 26.3 Beta"), task);
        dialog->setSuggestedIcon("minecraft");
        return;
    }
    if (!m_selectedVersion) {
        dialog->setSuggestedPack();
        return;
    }

    // There isn't a selected version if the version list is empty
    if (ui->loaderVersionList->selectedVersion() == nullptr)
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
    suggestCurrent();
    loaderFilterChanged();
}

void CustomPage::setSelectedLoaderVersion(BaseVersion::Ptr version)
{
    m_selectedLoaderVersion = version;
    suggestCurrent();
}
