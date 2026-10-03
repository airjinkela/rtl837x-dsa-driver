## RTL8372N DSA Driver
#### ~~Enjoy BUGS~~


### Example device tree
```C

// Example i2c-gpio over rtl8372n switch chip
i2c_rtk: i2c {
    compatible = "i2c-gpio";
    sda-gpios = <&rtk_gpio 39 GPIO_ACTIVE_HIGH>;
    scl-gpios = <&rtk_gpio 40 GPIO_ACTIVE_HIGH>;
    i2c-gpio,delay-us = <5>;
    i2c-gpio,timeout-ms = <1>;
};

// simple sfp example
sfp0: sfp {
    compatible = "sff,sfp";

    i2c-bus = <&i2c_rtk>;
    mod-def0-gpios = <&rtk_gpio 38 GPIO_ACTIVE_LOW>;
    maximum-power-milliwatt = <1000>;
};

&mdio {
	rtk_gpio: switch0: switch@1d {
		compatible = "realtek,rtl8372n";
		reg = <29>;

		reset-gpios = <&pio 42 GPIO_ACTIVE_HIGH>;

        // Serdes config
		sds0-rx-swap; # optional
		sds0-tx-swap; # optional
		sds1-rx-swap; # optional
		sds0-tx-swap; # optional

		phy-mdi-reverse; # optional
		phy-tx-polarity-swap; # optional

        // GPIO Controller
		gpio-controller; # optional
		#gpio-cells = <2>; # optional

		ports {
			#address-cells = <1>;
			#size-cells = <0>;

			/*
			* Port 0~2 is unused on rtl8372n switch chip
			*
			* if the chip is rtl8373(n) with rtl8224 
			* port3 will configure as a 'port' not a serdes port
			* (I haven't implemented the driver for this part yet :D)
			*/

			port@3 {
				reg = <3>;
				label = "cpu";
				ethernet = <&gmac0>;
				phy-mode = "10gbase-r";

				// Default DSA tag is "rtl8_4"
				// dsa-tag-protocol = "mxl862xx-8021q"; # Optional

				fixed-link {
					speed = <10000>;
					full-duplex;
					pause;
					asym-pause;
				};
			};

			port@4 {
				reg = <4>;
				label = "lan1";
				phy-mode = "internal";
				phy-handle = <&internal_phy1>;
			};

			port@5 {
				reg = <5>;
				label = "lan2";
				phy-mode = "internal";
				phy-handle = <&internal_phy2>;
			};

			port@6 {
				reg = <6>;
				label = "lan3";
				phy-mode = "internal";
				phy-handle = <&internal_phy3>;
			};

			port@7 {
				reg = <7>;
				label = "lan4";
				phy-mode = "internal";
				phy-handle = <&internal_phy4>;
			};

			port@8 {
				reg = <8>;
				label = "lan5";
				sfp = <&sfp0>;
				phy-mode = "10gbase-r";
				managed = "in-band-status";
			};
		};

		mdio {
			internal_phy1: phy@4 {
				compatible = "ethernet-phy-ieee802.3-c45";
				reg = <4>;
			};

			internal_phy2: phy@5 {
				compatible = "ethernet-phy-ieee802.3-c45";
				reg = <5>;
			};

			internal_phy3: phy@6 {
				compatible = "ethernet-phy-ieee802.3-c45";
				reg = <6>;
			};

			internal_phy4: phy@7 {
				compatible = "ethernet-phy-ieee802.3-c45";
				reg = <7>;
			};
		};
	};
};
```

## The current bug I have discovered
  1. ~~When ports are bridged on the same bridge, 
    if there are different VLANs, packets between 
	VLANs will leak to each other, even if the two 
	VLANs do not contain the same port~~ ***--FIXED***
  2. STP blocks ordinary traffic, but BPDUs can still flood between ports.
     This branch proposes CPU trapping for **`rtl8_4` only**; hardware
     validation is pending. With `mxl862xx-8021q`, trapped frames bypass
     S-VLAN processing and lose source-port metadata, so the STP issue
     remains unresolved. See [the diagnosis and review procedure](STP-BPDU-REPORT.md).
