#ifndef _RSUIDPWDPROPERTIES_H_
#define _RSUIDPWDPROPERTIES_H_

/// The connection key for looking up UID setting.
#define RS_UID_KEY L"UID"

/// The connection key for looking up AllowHostnameCNMismatch.
#define RS_PWD_KEY L"PWD"

/// @brief Captures information on how user name and password configurations are treated.
struct RsUidPwdProperties
{
// Public ======================================================================================
public:
    /// @brief Constructor.
    ///
    /// @param in_uidKey                The key name for the user name connection setting.
    ///                                 Defaults to DS_UID_KEY.
    /// @param in_pwdKey                The key name for the password connection setting.
    ///                                 Defaults to DS_PWD_KEY.
    /// @param in_uidPwdRequired        Indicates whether UID and PWD are treated as required
    ///                                 attributes. Default true.
    /// @param in_keyNamePrefix         They key name prefix. Pass in DS_NO_KEY_NAME_PREFIX to
    ///                                 indicate no key name prefix available. Default
    ///                                 DS_NO_KEY_NAME_PREFIX.
    explicit RsUidPwdProperties(
        const rs_wstring& in_uidKey = rs_wstring(RS_UID_KEY),
        const rs_wstring& in_pwdKey = rs_wstring(RS_PWD_KEY),
        bool in_uidPwdRequired = true) :
            m_uidKey(in_uidKey),
            m_pwdKey(in_pwdKey),
            m_isUidRequired(in_uidPwdRequired),
            m_isPwdRequired(in_uidPwdRequired)
    {
        ; // Do nothing.
    }

    /// @brief Constructor.
    ///
    /// @param in_isUidRequired         Indicates whether UID is treated as required attributes.
    /// @param in_isPwdRequired         Indicates whether PWD is treated as required attributes.
    /// @param in_uidKey                The key name for the user name connection setting.
    ///                                 Defaults to DS_UID_KEY.
    /// @param in_pwdKey                The key name for the password connection setting.
    ///                                 Defaults to DS_PWD_KEY.
    explicit RsUidPwdProperties(
        bool in_isUidRequired,
        bool in_isPwdRequired,
        const rs_wstring& in_uidKey = rs_wstring(RS_UID_KEY),
        const rs_wstring& in_pwdKey = rs_wstring(RS_PWD_KEY)) :
            m_uidKey(in_uidKey),
            m_pwdKey(in_pwdKey),
            m_isUidRequired(in_isUidRequired),
            m_isPwdRequired(in_isPwdRequired)
    {
        ; // Do nothing.
    }

    /// @breif Constructor.
    ///
    /// @param in_rhs                   The DSUidPwdProperties_ to copy from.
    explicit RsUidPwdProperties(const RsUidPwdProperties& in_rhs) :
        m_uidKey(in_rhs.m_uidKey),
        m_pwdKey(in_rhs.m_pwdKey),
        m_isUidRequired(in_rhs.m_isUidRequired),
        m_isPwdRequired(in_rhs.m_isPwdRequired)
    {
        ; // Do nothing.
    }

    /// @brief Destructor.
    ~RsUidPwdProperties()
    {
        ; // Do nothing.
    }


    /// @brief Gets the PWD Key.
    ///
    /// @return the PWD Key.
    rs_wstring GetPwdKey() const
    {
        return  m_pwdKey;
    }

    /// @brief Gets the UID Key.
    ///
    /// @return the UID Key.
    rs_wstring GetUidKey() const
    {
        return  m_uidKey;
    }

    // The key name for the user name connection setting. Defaults to DS_UID_KEY.
    const rs_wstring m_uidKey;

    // The key name for the password connection setting. Defaults to DS_PWD_KEY.
    const rs_wstring m_pwdKey;

    // Indicates whether UID is treated as a required attribute.
    const bool m_isUidRequired;

    // Indicates whether PWD is treated as a required attribute.
    const bool m_isPwdRequired;

// Private =====================================================================================
private:
    // Hidden default constructor.
    RsUidPwdProperties();

    // Hidden assignment operator.
    const RsUidPwdProperties& operator=(const RsUidPwdProperties&);
};


#endif
