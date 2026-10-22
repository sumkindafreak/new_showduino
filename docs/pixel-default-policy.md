# Pixel default policy

Programmable lines start at **10 pixels** and initialise during boot. A valid saved count takes precedence; zero, missing or invalid stored values fall back to 10. This applies to Audio Node GPIO22, C3 Pixel Node, C3 Emergency Node's programmable line, P4's show line, and the legacy Mega's programmable lines.

Fixed physical indicators retain their fitted counts: Audio status GPIO5 = 1, Director ambient = 2, MOSFET identifier = 4, fitted lamp Jewels = 7, and explicitly sized emergency signage. This rule distinguishes a programmable line's initial default from the number of LEDs physically fitted to an indicator.

Audio firmware 0.4.4 reports independent RMT readiness for GPIO5 and GPIO22, and reports failed writes rather than assuming `show()` succeeded. The GPIO22 count can be changed and initialised from the WebUI while the output is idle; effects, locate and emergency block local commissioning. Physical lighting still requires bench verification after flashing.
