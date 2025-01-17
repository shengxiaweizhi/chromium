// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_package_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_package_service.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniPackageOperation[] =
    "crostini.package_operation";

}  // namespace

CrostiniPackageNotification::NotificationSettings::NotificationSettings() {}
CrostiniPackageNotification::NotificationSettings::NotificationSettings(
    const NotificationSettings& rhs) = default;
CrostiniPackageNotification::NotificationSettings::~NotificationSettings() {}

CrostiniPackageNotification::CrostiniPackageNotification(
    Profile* profile,
    NotificationType notification_type,
    PackageOperationStatus status,
    const base::string16& app_name,
    const std::string& notification_id,
    CrostiniPackageService* package_service)
    : notification_type_(notification_type),
      current_status_(status),
      package_service_(package_service),
      profile_(profile),
      notification_settings_(
          GetNotificationSettingsForTypeAndAppName(notification_type,
                                                   app_name)),
      weak_ptr_factory_(this) {
  if (status == PackageOperationStatus::RUNNING) {
    running_start_time_ = base::Time::Now();
  }
  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &ash::kNotificationLinuxIcon;
  rich_notification_data.never_timeout = true;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      base::string16(), base::string16(),
      gfx::Image(),  // icon
      notification_settings_.source,
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCrostiniPackageOperation),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  // Sets title and body
  UpdateProgress(status, 0 /*progress_percent*/);
}

CrostiniPackageNotification::~CrostiniPackageNotification() = default;

// static
CrostiniPackageNotification::NotificationSettings
CrostiniPackageNotification::GetNotificationSettingsForTypeAndAppName(
    NotificationType notification_type,
    const base::string16& app_name) {
  NotificationSettings result;

  switch (notification_type) {
    case NotificationType::PACKAGE_INSTALL:
      DCHECK(app_name.empty());
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.progress_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_IN_PROGRESS_TITLE);
      result.progress_body.clear();
      result.success_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_TITLE);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_TITLE);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_PACKAGE_INSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    case NotificationType::APPLICATION_UNINSTALL:
      result.source = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_DISPLAY_SOURCE);
      result.queued_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_TITLE,
          app_name);
      result.queued_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_QUEUED_MESSAGE);
      result.progress_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_IN_PROGRESS_TITLE,
          app_name);
      result.success_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_TITLE,
          app_name);
      result.success_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_COMPLETED_MESSAGE);
      result.failure_title = l10n_util::GetStringFUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_TITLE,
          app_name);
      result.failure_body = l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_NOTIFICATION_ERROR_MESSAGE);
      break;

    default:
      NOTREACHED();
  }

  return result;
}

// TODO(timloh): This doesn't get called if the user shuts down Crostini, so
// the notification will be stuck at whatever percentage it is at.
void CrostiniPackageNotification::UpdateProgress(PackageOperationStatus status,
                                                 int progress_percent) {
  if (status == PackageOperationStatus::RUNNING &&
      current_status_ != PackageOperationStatus::RUNNING) {
    running_start_time_ = base::Time::Now();
  }
  current_status_ = status;

  base::string16 title;
  base::string16 body;
  message_center::NotificationType notification_type =
      message_center::NOTIFICATION_TYPE_SIMPLE;
  bool never_timeout = false;

  switch (status) {
    case PackageOperationStatus::SUCCEEDED:
      title = notification_settings_.success_title;
      body = notification_settings_.success_body;
      break;

    case PackageOperationStatus::FAILED:
      title = notification_settings_.failure_title;
      body = notification_settings_.failure_body;
      notification_->set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      break;

    case PackageOperationStatus::RUNNING:
      never_timeout = true;
      notification_type = message_center::NOTIFICATION_TYPE_PROGRESS;
      title = notification_settings_.progress_title;
      if (notification_type_ == NotificationType::APPLICATION_UNINSTALL) {
        // Uninstalls have a time remaining instead of a fixed message.
        base::TimeDelta time_since_started_running =
            base::Time::Now() - running_start_time_;

        // Don't estimate if we don't have enough data yet. At the moment we
        // start the uninstall, we have no idea how long it will take. Only
        // estimate once we've spent at least 3 seconds OR gotten 10% of the
        // way through the uninstall.
        constexpr base::TimeDelta kMinTimeForEstimate =
            base::TimeDelta::FromSeconds(3);
        constexpr base::TimeDelta kTimeDeltaZero =
            base::TimeDelta::FromSeconds(0);
        constexpr int kMinPercentForEstimate = 10;
        if ((time_since_started_running >= kMinTimeForEstimate &&
             progress_percent > 0) ||
            (progress_percent >= kMinPercentForEstimate &&
             time_since_started_running > kTimeDeltaZero)) {
          base::TimeDelta total_time_expected =
              (time_since_started_running * 100) / progress_percent;
          base::TimeDelta time_remaining =
              total_time_expected - time_since_started_running;
          body = ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_REMAINING,
                                        ui::TimeFormat::LENGTH_SHORT,
                                        time_remaining);
        }
        // else leave body blank
      } else {
        body = notification_settings_.progress_body;
      }
      break;

    case PackageOperationStatus::QUEUED:
      // We don't have queued strings for some NotificationTypes; we shouldn't
      // be asked to move to QUEUED status for those,
      DCHECK(!notification_settings_.queued_title.empty());
      DCHECK(!notification_settings_.queued_body.empty());
      title = notification_settings_.queued_title;
      body = notification_settings_.queued_body;
      break;

    default:
      NOTREACHED();
  }

  notification_->set_title(title);
  notification_->set_message(body);
  notification_->set_type(notification_type);
  notification_->set_progress(progress_percent);
  notification_->set_never_timeout(never_timeout);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::ForceAllowAutoHide() {
  notification_->set_never_timeout(false);
  UpdateDisplayedNotification();
}

void CrostiniPackageNotification::Close(bool by_user) {
  // This call deletes us.
  package_service_->NotificationClosed(this);
}

void CrostiniPackageNotification::UpdateDisplayedNotification() {
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT,
                           *notification_);
}

}  // namespace crostini
