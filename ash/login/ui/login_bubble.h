// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_BUBBLE_H_
#define ASH_LOGIN_UI_LOGIN_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/login/ui/login_base_bubble_view.h"
#include "base/strings/string16.h"
#include "components/user_manager/user_type.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
class LoginButton;
class LoginMenuView;

// A wrapper for the bubble view in the login screen.
class ASH_EXPORT LoginBubble : public views::WidgetObserver {
 public:
  class TestApi {
   public:
    explicit TestApi(LoginBaseBubbleView* bubble_view);
    views::View* user_menu_remove_user_button();
    views::View* remove_user_confirm_data();
    views::Label* username_label();

   private:
    LoginBaseBubbleView* bubble_view_;
  };

  static const int kUserMenuRemoveUserButtonIdForTest;

  LoginBubble();
  ~LoginBubble() override;

  // Shows an error bubble for authentication failure.
  // |anchor_view| is the anchor for placing the bubble view.
  void ShowErrorBubble(views::View* content,
                       views::View* anchor_view,
                       bool show_persistently);

  // Shows a user menu bubble.
  // |anchor_view| is the anchor for placing the bubble view.
  // |bubble_opener| is a view that could open/close the bubble.
  // |show_remove_user| indicate whether or not we show the
  // "Remove this user" action.
  void ShowUserMenu(const base::string16& username,
                    const base::string16& email,
                    user_manager::UserType type,
                    bool is_owner,
                    views::View* anchor_view,
                    LoginButton* bubble_opener,
                    bool show_remove_user,
                    base::OnceClosure on_remove_user_warning_shown,
                    base::OnceClosure on_remove_user_requested);

  // Shows a tooltip.
  void ShowTooltip(const base::string16& message, views::View* anchor_view);

  // Shows a selection menu.
  void ShowSelectionMenu(LoginMenuView* menu);

  // Schedule animation for closing the bubble.
  // The bubble widget will be closed when the animation is ended.
  void Close();

  // Close the bubble immediately, without scheduling animation.
  // Used to clean up old bubble widget when a new bubble is going to be
  // created or it will be called before anchor view is hidden.
  void CloseImmediately();

  // True if the bubble is visible.
  bool IsVisible();

  LoginBaseBubbleView* bubble_view() { return bubble_view_; }

 private:
  LoginBaseBubbleView* bubble_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginBubble);
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_BUBBLE_H_
