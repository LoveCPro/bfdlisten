include $(TOPDIR)/rules.mk

PKG_NAME:=bfd
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_MAINTAINER:=dandan
PKG_LICENSE:=dan

include $(INCLUDE_DIR)/package.mk

define Package/bfd
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=+libblobmsg-json +libnl +libuci +libpthread +libubox +libubus  
	TITLE:=bfd check scripts
	MAINTAINER:=dandan
	PKGARCH:=all
endef

define Package/bfd/description
	Scripts that used for starting bfd tests and starting listen bfd status
endef

define Package/bfd/conffiles
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/gpn-bfd/install
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/etc/config/bfd.config $(1)/etc/config/bfd
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) ./files/etc/init.d/bfd.init $(1)/etc/init.d/bfd
	$(INSTALL_DIR) $(1)/etc/hotplug.d/bfd
	$(INSTALL_BIN) ./files/etc/hotplug.d/bfd/10-bfd $(1)/etc/hotplug.d/bfd/10-bfd
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/bfdlisten $(1)/usr/sbin/bfdlisten
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
