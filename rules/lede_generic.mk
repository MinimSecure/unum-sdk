# (c) 2019-2022 minim.co
# Makefile for compiling unum and installing necessary files
# on lede_generic devices

####################################################################
# Lists of components to build and install for the platform        #
####################################################################

TARGET_LIST := unum
TARGET_INSTALL_LIST := $(TARGET_LIST) files

####################################################################

# install utils
INSTALL_BIN:=install -m0755
INSTALL_DIR:=install -d -m0755
INSTALL_DATA:=install -m0644


unum.install:
	$(INSTALL_DIR) "$(TARGET_RFS)/usr/bin"
	$(STRIP) -o "$(TARGET_RFS)/usr/bin/unum" "$(TARGET_OBJ)/unum/unum"

files.install:
	# Create target directories
	$(INSTALL_DIR) $(TARGET_RFS)/sbin
	$(INSTALL_DIR) $(TARGET_RFS)/usr/bin
	$(INSTALL_DIR) $(TARGET_RFS)/etc/unum
	$(INSTALL_DIR) $(TARGET_RFS)/etc/init.d
	$(INSTALL_DIR) $(TARGET_RFS)/etc/ssl/certs
	$(INSTALL_DIR) $(TARGET_RFS)/etc/unum/.ssh
	$(INSTALL_DIR) $(TARGET_RFS)/lib/upgrade/keep.d

	# Add a few files to the list of files that persist over upgrade
	echo "/etc/unum/unum.pem" > $(TARGET_RFS)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/unum.key" >> $(TARGET_RFS)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/user.conf" >> $(TARGET_RFS)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/.wifi_provisioned" >> $(TARGET_RFS)/lib/upgrade/keep.d/unum.upgrade
	echo "/etc/unum/.credentials_provisioned" >> $(TARGET_RFS)/lib/upgrade/keep.d/unum.upgrade

	# Install service files and misc scripts
	$(INSTALL_BIN) $(TARGET_FILES)/unum.init $(TARGET_RFS)/etc/init.d/unum
	$(INSTALL_BIN) $(TARGET_FILES)/fix_lan_addr.init $(TARGET_RFS)/etc/init.d/fix_lan_addr
	$(INSTALL_BIN) $(TARGET_FILES)/unum-updater.init $(TARGET_RFS)/etc/init.d/unum-updater
	$(INSTALL_BIN) $(TARGET_FILES)/restart_config.init $(TARGET_RFS)/etc/init.d/restart_config

	$(INSTALL_DATA) $(TARGET_FILES)/unum.common $(TARGET_RFS)/etc/unum/unum.common
	$(INSTALL_BIN) $(TARGET_FILES)/fix_lan_addr.sh $(TARGET_RFS)/usr/bin/fix_lan_addr.sh
	$(INSTALL_BIN) $(TARGET_FILES)/restart_config.sh $(TARGET_RFS)/usr/bin/restart_config.sh

	$(INSTALL_BIN) $(TARGET_FILES)/switch_mode.sh $(TARGET_RFS)/sbin/switch_mode.sh
	$(INSTALL_BIN) $(TARGET_FILES)/run_on_board_sta.sh $(TARGET_RFS)/sbin/run_on_board_sta.sh
	$(INSTALL_BIN) $(TARGET_FILES)/create_on_board_sta.sh $(TARGET_RFS)/sbin/create_on_board_sta.sh

	# Trusted CA list
	cat "$(COMMON_FILES)/ca/"*.pem > "$(TARGET_RFS)/etc/ssl/certs/unum-ca-certificates.crt"

	# ssh keys
	$(INSTALL_DATA) $(COMMON_FILES)/ssh_keys/* $(TARGET_RFS)/etc/unum/.ssh/
	$(INSTALL_DATA) $(COMMON_FILES)/ssh_keys/id_rsa $(TARGET_RFS)/etc/unum/.ssh
	$(INSTALL_DATA) $(COMMON_FILES)/ssh_keys/known_hosts $(TARGET_RFS)/etc/unum/.ssh
	$(INSTALL_DATA) $(COMMON_FILES)/ssh_keys/authorized_keys $(TARGET_RFS)/etc/unum/.ssh

	# Hardware specific files to set default password and default SSID and key
	if [ -d $(TARGET_FILES)/$(HARDWARE)/. ]; then \
		cp -r $(TARGET_FILES)/$(HARDWARE)/* $(TARGET_RFS); \
	fi

	# Default agent config file
	echo "export UNUM_CA_FILE=/etc/ssl/certs/unum-ca-certificates.crt" > $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_SSH_DIR=/etc/unum/.ssh" >> $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_SSH_AKEYS=/etc/unum/.ssh/authorized_keys" >> $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_SSH_MYKEY=/etc/unum/.ssh/id_rsa" >> $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_SSH_KNOWN=/etc/unum/.ssh/known_hosts" >> $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_PID_FILE_PREFIX=/var/run/unum" >> $(TARGET_RFS)/etc/unum/system.conf
	echo "export UNUM_NICENESS=13" >> $(TARGET_RFS)/etc/unum/system.conf

