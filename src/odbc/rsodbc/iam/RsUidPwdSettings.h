#ifndef _RS_UID_PWD_SETTINGS_H_
#define _RS_UID_PWD_SETTINGS_H_

/// @brief User Name and Password related connection settings.
struct RsUidPwdSettings
{
  RsUidPwdSettings()
  {

  }

  // The UID value.
  rs_string m_uid;

  // The PWD value.
  rs_string m_pwd;

};

#endif
