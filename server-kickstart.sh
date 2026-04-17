#!/bin/bash

# =================================================================
# Ubuntu Server Headless Stability Script
# 目的: モニター/キーボードなしでの24時間安定稼働を実現する
# =================================================================

set -e

echo " Server hardening process started..."

# 1. カーネルパラメータの最適化
# nomodeset: ビデオドライバのパニック防止
# intel_idle/processor.max_cstate=1: CPUの深すぎるスリープによるフリーズ防止
echo " Optimizing GRUB configuration..."
sudo sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT="maybe-ubiquity nomodeset intel_idle.max_cstate=1 processor.max_cstate=1"/' /etc/default/grub
sudo update-grub

# 2. 不要な物理デバイスドライバのブラックリスト化
# snd_hda_intel: HDMI切断時にシステムをクラッシュさせるオーディオドライバを封印
echo " Blacklisting unstable kernel modules (Audio/HDMI)..."
sudo bash -c 'cat << EOF > /etc/modprobe.d/blacklist-headless.conf
blacklist snd_hda_intel
blacklist snd_hda_codec_hdmi
blacklist snd_hda_core
EOF'
sudo update-initramfs -u

# 3. 物理スイッチ・電源管理の制御
# HandleLidSwitch: ノートPCのフタを閉めても稼働し続ける設定
echo " Configuring logind to ignore physical lid switches..."
sudo mkdir -p /etc/systemd/logind.conf.d/
sudo bash -c 'cat << EOF > /etc/systemd/logind.conf.d/99-headless.conf
[Login]
HandleLidSwitch=ignore
HandleLidSwitchExternalPower=ignore
HandleLidSwitchDocked=ignore
LidSwitchIgnoreInhibited=no
EOF'
sudo systemctl restart systemd-logind

# 4. OSレベルのスリープ機能を完全無効化
# サーバーが勝手にサスペンドに入るのを物理的に阻止する
echo " Masking all sleep and suspend targets..."
sudo systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target

# 5. CUI（マルチユーザー）ターゲットの固定
# GUIが入っているエディションでも、確実にコマンドライン起動にする
echo "  Setting default target to multi-user (CLI)..."
sudo systemctl set-default multi-user.target

echo "-------------------------------------------------------"
echo " Hardening Complete!"
echo " Your server is now ready for 24/7 headless operation."
echo " Please run 'sudo reboot' to apply all changes."
echo "-------------------------------------------------------"
