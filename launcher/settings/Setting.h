#pragma once

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <memory>

class SettingsObject;

/*!
 *
 */
class Setting : public QObject {
    Q_OBJECT
   public:
    /**
     * Construct a Setting
     *
     * Synonyms are all the possible names used in the settings object, in order of preference.
     * First synonym is the ID, which identifies the setting in PolyMC.
     *
     * defVal is the default value that will be returned when the settings object
     * doesn't have any value for this setting.
     */
    explicit Setting(QStringList synonyms, QVariant defVal = QVariant());

    /*!
     * \brief Gets this setting's ID.
     * This is used to refer to the setting within the application.
     * \warning Changing the ID while the setting is registered with a SettingsObject results in
     * undefined behavior.
     * \return The ID of the setting.
     */
    virtual QString id() const { return m_synonyms.first(); }

    /*!
     * \brief Gets this setting's config file key.
     * This is used to store the setting's value in the config file. It is usually
     * the same as the setting's ID, but it can be different.
     * \return The setting's config file key.
     */
    virtual QStringList configKeys() const { return m_synonyms; }

    /*!
     * \brief Gets this setting's value as a QVariant.
     * This is done by calling the SettingsObject's retrieveValue() function.
     * If this Setting doesn't have a SettingsObject, this returns an invalid QVariant.
     * \return QVariant containing this setting's value.
     * \sa value()
     */
    virtual QVariant get() const;

    /*!
     * \brief Gets this setting's default value.
     * \return The default value of this setting.
     */
    virtual QVariant defValue() const;

   signals:
    /*!
     * \brief Signal emitted when this Setting object's value changes.
     * \param setting A reference to the Setting that changed.
     * \param value This Setting object's new value.
     */
    void SettingChanged(const Setting& setting, QVariant value);

    /*!
     * \brief Signal emitted when this Setting object's value resets to default.
     * \param setting A reference to the Setting that changed.
     */
    void settingReset(const Setting& setting);

   public slots:
    /*!
     * \brief Changes the setting's value.
     * This is done by emitting the SettingChanged() signal which will then be
     * handled by the SettingsObject object and cause the setting to change.
     * \param value The new value.
     */
    virtual void set(QVariant value);

    /*!
     * \brief Reset the setting to default
     * This is done by emitting the settingReset() signal which will then be
     * handled by the SettingsObject object and cause the setting to change.
     */
    virtual void reset();

   protected:
    friend class SettingsObject;
    SettingsObject* m_storage;
    QStringList m_synonyms;
    QVariant m_defVal;
};
