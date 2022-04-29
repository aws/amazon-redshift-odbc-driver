/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0.
 */

#pragma once
#include <aws/redshiftarcadiacoral/RedshiftArcadiaCoralService_EXPORTS.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/core/utils/DateTime.h>
#include <utility>

namespace Aws
{
namespace Utils
{
namespace Json
{
  class JsonValue;
  class JsonView;
} // namespace Json
} // namespace Utils
namespace RedshiftArcadiaCoralService
{
namespace Model
{

  class AWS_REDSHIFTARCADIACORALSERVICE_API Snapshot
  {
  public:
    Snapshot();
    Snapshot(Aws::Utils::Json::JsonView jsonValue);
    Snapshot& operator=(Aws::Utils::Json::JsonView jsonValue);
    Aws::Utils::Json::JsonValue Jsonize() const;


    
    inline double GetActualIncrementalBackupSizeInMegaBytes() const{ return m_actualIncrementalBackupSizeInMegaBytes; }

    
    inline bool ActualIncrementalBackupSizeInMegaBytesHasBeenSet() const { return m_actualIncrementalBackupSizeInMegaBytesHasBeenSet; }

    
    inline void SetActualIncrementalBackupSizeInMegaBytes(double value) { m_actualIncrementalBackupSizeInMegaBytesHasBeenSet = true; m_actualIncrementalBackupSizeInMegaBytes = value; }

    
    inline Snapshot& WithActualIncrementalBackupSizeInMegaBytes(double value) { SetActualIncrementalBackupSizeInMegaBytes(value); return *this;}


    
    inline double GetBackupProgressInMegaBytes() const{ return m_backupProgressInMegaBytes; }

    
    inline bool BackupProgressInMegaBytesHasBeenSet() const { return m_backupProgressInMegaBytesHasBeenSet; }

    
    inline void SetBackupProgressInMegaBytes(double value) { m_backupProgressInMegaBytesHasBeenSet = true; m_backupProgressInMegaBytes = value; }

    
    inline Snapshot& WithBackupProgressInMegaBytes(double value) { SetBackupProgressInMegaBytes(value); return *this;}


    
    inline double GetCurrentBackupRateInMegaBytesPerSecond() const{ return m_currentBackupRateInMegaBytesPerSecond; }

    
    inline bool CurrentBackupRateInMegaBytesPerSecondHasBeenSet() const { return m_currentBackupRateInMegaBytesPerSecondHasBeenSet; }

    
    inline void SetCurrentBackupRateInMegaBytesPerSecond(double value) { m_currentBackupRateInMegaBytesPerSecondHasBeenSet = true; m_currentBackupRateInMegaBytesPerSecond = value; }

    
    inline Snapshot& WithCurrentBackupRateInMegaBytesPerSecond(double value) { SetCurrentBackupRateInMegaBytesPerSecond(value); return *this;}


    
    inline const Aws::String& GetDbName() const{ return m_dbName; }

    
    inline bool DbNameHasBeenSet() const { return m_dbNameHasBeenSet; }

    
    inline void SetDbName(const Aws::String& value) { m_dbNameHasBeenSet = true; m_dbName = value; }

    
    inline void SetDbName(Aws::String&& value) { m_dbNameHasBeenSet = true; m_dbName = std::move(value); }

    
    inline void SetDbName(const char* value) { m_dbNameHasBeenSet = true; m_dbName.assign(value); }

    
    inline Snapshot& WithDbName(const Aws::String& value) { SetDbName(value); return *this;}

    
    inline Snapshot& WithDbName(Aws::String&& value) { SetDbName(std::move(value)); return *this;}

    
    inline Snapshot& WithDbName(const char* value) { SetDbName(value); return *this;}


    
    inline long long GetElapsedTimeInSeconds() const{ return m_elapsedTimeInSeconds; }

    
    inline bool ElapsedTimeInSecondsHasBeenSet() const { return m_elapsedTimeInSecondsHasBeenSet; }

    
    inline void SetElapsedTimeInSeconds(long long value) { m_elapsedTimeInSecondsHasBeenSet = true; m_elapsedTimeInSeconds = value; }

    
    inline Snapshot& WithElapsedTimeInSeconds(long long value) { SetElapsedTimeInSeconds(value); return *this;}


    
    inline bool GetEncrypted() const{ return m_encrypted; }

    
    inline bool EncryptedHasBeenSet() const { return m_encryptedHasBeenSet; }

    
    inline void SetEncrypted(bool value) { m_encryptedHasBeenSet = true; m_encrypted = value; }

    
    inline Snapshot& WithEncrypted(bool value) { SetEncrypted(value); return *this;}


    
    inline long long GetEstimatedSecondsToCompletion() const{ return m_estimatedSecondsToCompletion; }

    
    inline bool EstimatedSecondsToCompletionHasBeenSet() const { return m_estimatedSecondsToCompletionHasBeenSet; }

    
    inline void SetEstimatedSecondsToCompletion(long long value) { m_estimatedSecondsToCompletionHasBeenSet = true; m_estimatedSecondsToCompletion = value; }

    
    inline Snapshot& WithEstimatedSecondsToCompletion(long long value) { SetEstimatedSecondsToCompletion(value); return *this;}


    
    inline const Aws::String& GetKmsKeyId() const{ return m_kmsKeyId; }

    
    inline bool KmsKeyIdHasBeenSet() const { return m_kmsKeyIdHasBeenSet; }

    
    inline void SetKmsKeyId(const Aws::String& value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId = value; }

    
    inline void SetKmsKeyId(Aws::String&& value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId = std::move(value); }

    
    inline void SetKmsKeyId(const char* value) { m_kmsKeyIdHasBeenSet = true; m_kmsKeyId.assign(value); }

    
    inline Snapshot& WithKmsKeyId(const Aws::String& value) { SetKmsKeyId(value); return *this;}

    
    inline Snapshot& WithKmsKeyId(Aws::String&& value) { SetKmsKeyId(std::move(value)); return *this;}

    
    inline Snapshot& WithKmsKeyId(const char* value) { SetKmsKeyId(value); return *this;}


    
    inline int GetManualSnapshotRemainingDays() const{ return m_manualSnapshotRemainingDays; }

    
    inline bool ManualSnapshotRemainingDaysHasBeenSet() const { return m_manualSnapshotRemainingDaysHasBeenSet; }

    
    inline void SetManualSnapshotRemainingDays(int value) { m_manualSnapshotRemainingDaysHasBeenSet = true; m_manualSnapshotRemainingDays = value; }

    
    inline Snapshot& WithManualSnapshotRemainingDays(int value) { SetManualSnapshotRemainingDays(value); return *this;}


    
    inline int GetManualSnapshotRetentionPeriod() const{ return m_manualSnapshotRetentionPeriod; }

    
    inline bool ManualSnapshotRetentionPeriodHasBeenSet() const { return m_manualSnapshotRetentionPeriodHasBeenSet; }

    
    inline void SetManualSnapshotRetentionPeriod(int value) { m_manualSnapshotRetentionPeriodHasBeenSet = true; m_manualSnapshotRetentionPeriod = value; }

    
    inline Snapshot& WithManualSnapshotRetentionPeriod(int value) { SetManualSnapshotRetentionPeriod(value); return *this;}


    
    inline const Aws::String& GetMasterUsername() const{ return m_masterUsername; }

    
    inline bool MasterUsernameHasBeenSet() const { return m_masterUsernameHasBeenSet; }

    
    inline void SetMasterUsername(const Aws::String& value) { m_masterUsernameHasBeenSet = true; m_masterUsername = value; }

    
    inline void SetMasterUsername(Aws::String&& value) { m_masterUsernameHasBeenSet = true; m_masterUsername = std::move(value); }

    
    inline void SetMasterUsername(const char* value) { m_masterUsernameHasBeenSet = true; m_masterUsername.assign(value); }

    
    inline Snapshot& WithMasterUsername(const Aws::String& value) { SetMasterUsername(value); return *this;}

    
    inline Snapshot& WithMasterUsername(Aws::String&& value) { SetMasterUsername(std::move(value)); return *this;}

    
    inline Snapshot& WithMasterUsername(const char* value) { SetMasterUsername(value); return *this;}


    
    inline const Aws::String& GetOwnerAccount() const{ return m_ownerAccount; }

    
    inline bool OwnerAccountHasBeenSet() const { return m_ownerAccountHasBeenSet; }

    
    inline void SetOwnerAccount(const Aws::String& value) { m_ownerAccountHasBeenSet = true; m_ownerAccount = value; }

    
    inline void SetOwnerAccount(Aws::String&& value) { m_ownerAccountHasBeenSet = true; m_ownerAccount = std::move(value); }

    
    inline void SetOwnerAccount(const char* value) { m_ownerAccountHasBeenSet = true; m_ownerAccount.assign(value); }

    
    inline Snapshot& WithOwnerAccount(const Aws::String& value) { SetOwnerAccount(value); return *this;}

    
    inline Snapshot& WithOwnerAccount(Aws::String&& value) { SetOwnerAccount(std::move(value)); return *this;}

    
    inline Snapshot& WithOwnerAccount(const char* value) { SetOwnerAccount(value); return *this;}


    
    inline const Aws::Utils::DateTime& GetSnapshotCreateTime() const{ return m_snapshotCreateTime; }

    
    inline bool SnapshotCreateTimeHasBeenSet() const { return m_snapshotCreateTimeHasBeenSet; }

    
    inline void SetSnapshotCreateTime(const Aws::Utils::DateTime& value) { m_snapshotCreateTimeHasBeenSet = true; m_snapshotCreateTime = value; }

    
    inline void SetSnapshotCreateTime(Aws::Utils::DateTime&& value) { m_snapshotCreateTimeHasBeenSet = true; m_snapshotCreateTime = std::move(value); }

    
    inline Snapshot& WithSnapshotCreateTime(const Aws::Utils::DateTime& value) { SetSnapshotCreateTime(value); return *this;}

    
    inline Snapshot& WithSnapshotCreateTime(Aws::Utils::DateTime&& value) { SetSnapshotCreateTime(std::move(value)); return *this;}


    
    inline const Aws::String& GetSnapshotIdentifier() const{ return m_snapshotIdentifier; }

    
    inline bool SnapshotIdentifierHasBeenSet() const { return m_snapshotIdentifierHasBeenSet; }

    
    inline void SetSnapshotIdentifier(const Aws::String& value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier = value; }

    
    inline void SetSnapshotIdentifier(Aws::String&& value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier = std::move(value); }

    
    inline void SetSnapshotIdentifier(const char* value) { m_snapshotIdentifierHasBeenSet = true; m_snapshotIdentifier.assign(value); }

    
    inline Snapshot& WithSnapshotIdentifier(const Aws::String& value) { SetSnapshotIdentifier(value); return *this;}

    
    inline Snapshot& WithSnapshotIdentifier(Aws::String&& value) { SetSnapshotIdentifier(std::move(value)); return *this;}

    
    inline Snapshot& WithSnapshotIdentifier(const char* value) { SetSnapshotIdentifier(value); return *this;}


    
    inline const Aws::Utils::DateTime& GetSnapshotRetentionStartTime() const{ return m_snapshotRetentionStartTime; }

    
    inline bool SnapshotRetentionStartTimeHasBeenSet() const { return m_snapshotRetentionStartTimeHasBeenSet; }

    
    inline void SetSnapshotRetentionStartTime(const Aws::Utils::DateTime& value) { m_snapshotRetentionStartTimeHasBeenSet = true; m_snapshotRetentionStartTime = value; }

    
    inline void SetSnapshotRetentionStartTime(Aws::Utils::DateTime&& value) { m_snapshotRetentionStartTimeHasBeenSet = true; m_snapshotRetentionStartTime = std::move(value); }

    
    inline Snapshot& WithSnapshotRetentionStartTime(const Aws::Utils::DateTime& value) { SetSnapshotRetentionStartTime(value); return *this;}

    
    inline Snapshot& WithSnapshotRetentionStartTime(Aws::Utils::DateTime&& value) { SetSnapshotRetentionStartTime(std::move(value)); return *this;}


    
    inline const Aws::String& GetSourceRegion() const{ return m_sourceRegion; }

    
    inline bool SourceRegionHasBeenSet() const { return m_sourceRegionHasBeenSet; }

    
    inline void SetSourceRegion(const Aws::String& value) { m_sourceRegionHasBeenSet = true; m_sourceRegion = value; }

    
    inline void SetSourceRegion(Aws::String&& value) { m_sourceRegionHasBeenSet = true; m_sourceRegion = std::move(value); }

    
    inline void SetSourceRegion(const char* value) { m_sourceRegionHasBeenSet = true; m_sourceRegion.assign(value); }

    
    inline Snapshot& WithSourceRegion(const Aws::String& value) { SetSourceRegion(value); return *this;}

    
    inline Snapshot& WithSourceRegion(Aws::String&& value) { SetSourceRegion(std::move(value)); return *this;}

    
    inline Snapshot& WithSourceRegion(const char* value) { SetSourceRegion(value); return *this;}


    
    inline const Aws::String& GetStatus() const{ return m_status; }

    
    inline bool StatusHasBeenSet() const { return m_statusHasBeenSet; }

    
    inline void SetStatus(const Aws::String& value) { m_statusHasBeenSet = true; m_status = value; }

    
    inline void SetStatus(Aws::String&& value) { m_statusHasBeenSet = true; m_status = std::move(value); }

    
    inline void SetStatus(const char* value) { m_statusHasBeenSet = true; m_status.assign(value); }

    
    inline Snapshot& WithStatus(const Aws::String& value) { SetStatus(value); return *this;}

    
    inline Snapshot& WithStatus(Aws::String&& value) { SetStatus(std::move(value)); return *this;}

    
    inline Snapshot& WithStatus(const char* value) { SetStatus(value); return *this;}


    
    inline double GetTotalBackupSizeInMegaBytes() const{ return m_totalBackupSizeInMegaBytes; }

    
    inline bool TotalBackupSizeInMegaBytesHasBeenSet() const { return m_totalBackupSizeInMegaBytesHasBeenSet; }

    
    inline void SetTotalBackupSizeInMegaBytes(double value) { m_totalBackupSizeInMegaBytesHasBeenSet = true; m_totalBackupSizeInMegaBytes = value; }

    
    inline Snapshot& WithTotalBackupSizeInMegaBytes(double value) { SetTotalBackupSizeInMegaBytes(value); return *this;}


    
    inline const Aws::String& GetVpcId() const{ return m_vpcId; }

    
    inline bool VpcIdHasBeenSet() const { return m_vpcIdHasBeenSet; }

    
    inline void SetVpcId(const Aws::String& value) { m_vpcIdHasBeenSet = true; m_vpcId = value; }

    
    inline void SetVpcId(Aws::String&& value) { m_vpcIdHasBeenSet = true; m_vpcId = std::move(value); }

    
    inline void SetVpcId(const char* value) { m_vpcIdHasBeenSet = true; m_vpcId.assign(value); }

    
    inline Snapshot& WithVpcId(const Aws::String& value) { SetVpcId(value); return *this;}

    
    inline Snapshot& WithVpcId(Aws::String&& value) { SetVpcId(std::move(value)); return *this;}

    
    inline Snapshot& WithVpcId(const char* value) { SetVpcId(value); return *this;}

  private:

    double m_actualIncrementalBackupSizeInMegaBytes;
    bool m_actualIncrementalBackupSizeInMegaBytesHasBeenSet;

    double m_backupProgressInMegaBytes;
    bool m_backupProgressInMegaBytesHasBeenSet;

    double m_currentBackupRateInMegaBytesPerSecond;
    bool m_currentBackupRateInMegaBytesPerSecondHasBeenSet;

    Aws::String m_dbName;
    bool m_dbNameHasBeenSet;

    long long m_elapsedTimeInSeconds;
    bool m_elapsedTimeInSecondsHasBeenSet;

    bool m_encrypted;
    bool m_encryptedHasBeenSet;

    long long m_estimatedSecondsToCompletion;
    bool m_estimatedSecondsToCompletionHasBeenSet;

    Aws::String m_kmsKeyId;
    bool m_kmsKeyIdHasBeenSet;

    int m_manualSnapshotRemainingDays;
    bool m_manualSnapshotRemainingDaysHasBeenSet;

    int m_manualSnapshotRetentionPeriod;
    bool m_manualSnapshotRetentionPeriodHasBeenSet;

    Aws::String m_masterUsername;
    bool m_masterUsernameHasBeenSet;

    Aws::String m_ownerAccount;
    bool m_ownerAccountHasBeenSet;

    Aws::Utils::DateTime m_snapshotCreateTime;
    bool m_snapshotCreateTimeHasBeenSet;

    Aws::String m_snapshotIdentifier;
    bool m_snapshotIdentifierHasBeenSet;

    Aws::Utils::DateTime m_snapshotRetentionStartTime;
    bool m_snapshotRetentionStartTimeHasBeenSet;

    Aws::String m_sourceRegion;
    bool m_sourceRegionHasBeenSet;

    Aws::String m_status;
    bool m_statusHasBeenSet;

    double m_totalBackupSizeInMegaBytes;
    bool m_totalBackupSizeInMegaBytesHasBeenSet;

    Aws::String m_vpcId;
    bool m_vpcIdHasBeenSet;
  };

} // namespace Model
} // namespace RedshiftArcadiaCoralService
} // namespace Aws
