# CLOCKY

# !!IMPORTANT NOTICE!!

Dear reviewer, I live in Peoria, AZ, and I was planning a large final commit on Sunday because of a vast amount of work I had this week (school and PSAT and SAT), but the time zones caused it to close. On Half Life, the amount of hours shown is 6, but I actually worked 16 hours, as shown by the methodology and pictures below. Please take this into consideration. Thank you!

## Overview

Clocky is the project for week 5 of Half Life by Hack Club. It is a mini clock keychain to be hung on a backpack using the latest technology, e-paper. I want it to need no charging, so choosing e-paper was my first idea. It is powered by a solar cell and RF harvesting from an antenna. All this powers an RTC, which powers an MCU that writes to the display. The display uses a charge booster to draw enough power to write itself.

## Methodology

I first began by solving the power issue. My first thought was solar, but what if I left my clock in the dark for extended periods? Then I thought about RF harvesting, but it was only effective in places with high Wi-Fi yield, like houses. I decided to combine the two. The plan was to then store the voltage in a lithium-ion battery, but that might have been overkill, so I just settled for a supercapacitor, as it was much faster to charge and discharge. Then I went on LCSC and found extremely low-power RTCs and MCU's. I used Digi-Key for the e-paper and supercapacitor because they weren't available on LCSC.

## Images


<img width="585" height="473" alt="image" src="https://github.com/user-attachments/assets/7092aca3-5af2-432b-97be-3cc52e815a88" />

<img width="1391" height="772" alt="image" src="https://github.com/user-attachments/assets/b341a6e0-c7f2-4df5-aab8-828a4d3c6195" />
