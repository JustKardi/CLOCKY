# CLOCKY

## Overview

Clocky is the project for week 5 of Half Life by Hack Club. It is a mini clock keychain to be hung on a backpack using the latest technology, e-paper. I want it to need no charging, so choosing e-paper was my first idea. It is powered by a solar cell and RF harvesting from an antenna. All this powers an RTC, which powers an MCU that writes to the display. The display uses a charge booster to draw enough power to write itself.

## Methodology

I first began by solving the power issue. My first thought was solar, but what if I left my clock in the dark for extended periods? Then I thought about RF harvesting, but it was only effective in places with high Wi-Fi yield, like houses. I decided to combine the two. The plan was to then store the voltage in a lithium-ion battery, but that might have been overkill, so I just settled for a supercapacitor, as it was much faster to charge and discharge. Then I went on LCSC and found extremely low-power RTCs and MCU's. I used Digi-Key for the e-paper and supercapacitor because they weren't available on LCSC.
