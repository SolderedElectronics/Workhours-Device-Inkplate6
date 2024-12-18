# Inkplate Time Logger Device - Assembly Instructions

## Needed materials
- [Soldered Inkplate 6 without case - 333232](https://solde.red/333232)
- [Soldered RFID 125 kHz Breakout for UART - 333154](https://solde.red/333154)
- [125 kHz RFID tags](https://solde.red/108789)
- [Red prototyping wire](https://solde.red/333337)
- [Black prototyping wire](https://solde.red/333338)
- [White or yellow prototyping wire](https://solde.red/333339)
- [Generic NMOS transistor in SOT23-3 case](https://solde.red/101349)
- [330R SMD resistor in 0603 case](https://solde.red/101807)
- [10k SMD resistor in 0603 case](https://solde.red/101307)
- Schottky or [Silicon Diode](https://solde.red/101889)
- [Electromagnetic buzzer](https://solde.red/101247) or simmilar
- *Optional* [IPX to SMA adapter](https://solde.red/108591)
- *Optional* [2.4 GHz SMA Antenna](https://solde.red/108948) or simmilar
- *Optional* [IPX connector](https://solde.red/108942) or simmilar
- 3D printed case
- microSD Card 32GB max formatted as FAT32

## Needed tools
- Soldering iron or soldering station with fine tip
- [Soldering wire](https://solde.red/333057)
- *Optional* [Tweezers](https://solde.red/108319)
- [Solder Wick](https://solde.red/101697) *if things do not go as planned*
- Some double sided tape (2mm foam double sided tape)
- Lots of patience
- USB C cable
- *Optional* [USB power supply 5V 2A](https://solde.red/100717)
- [Cutting pliers](https://solde.red/100386)

## Assembly instructions
1. Print the desired case; there are two options; with external WiFi antenna for better reception, or without external antenna. We choose to use with the extenal antenna to get more reliable connection.

2. Compile the sketch! Download all needed libraries and set all settings described [here](https://github.com/SolderedElectronics/Workhours-Device-Inkplate6?tab=readme-ov-file#getting-started).

3. Place the antenna inside the case. **NOTE** The fit is very tight, so be carefull to not damage the antenna. We also used ferrite pad to get a little bit better RFID reception, but you don't need to do this.

4. Snip the wires of the RFID connector on the breakout PCB so you can easly tape the breakout on the case.

5. Cut the piece of double sided tape same as the size of the RFID breakout at tape inside of the case (see image).

6. Connect RFID Antenna and RFID breakout.

7. Close RFID Antenna. **NOTE** Since everything is a tight-fit, be very careful to not damage the RFID antenna wile closing it!

8. Cut the three wires for the RFID about 15cm long. Use red for RFID supply, black for the `GND` and white, yellow, or any other color for the UART.

9. Remove about few millimeters of the insulation from wires on the both sides.

10. Pre-tin them for easier soldering.

11. Solder black wire on the `GND` on RFID breakout, red on `VCC` and white or yellow to `TXD`.

12. Change the baud rate of the RFID Brekaout by switching 1 and 3 on DIP switch to ON position.

12. Solder red wire from RFID to `VIN`, black to `GND` and yellow or white to `GPIO39` on the Inkplate 6.

13. Cut the wires on the buzzer and add diode **NOTE** Watch for the polarity! Anode must be connected to `-` and cathode to `+` of the buzzer. Also cut the sticky tape if the needed to fix the buzzer in the place. Wrap it around the buzzer.

14. Cut one black and one red wire 15 cm long. Remove insulation and pre-tin them like before. This will be used for the buzzer.

15. Solder red wire to the `+`of the buzzer and black wire to the `-` of the buzzer.

16. Now comes the tricky part! We need to solder few SMD components. You can use THT, but note that you need to fix them in position with the hot glue or something simmilar.
   - First solder `NMOS` to the GND on the Inkplate's `GPIO Expander 1` header.
   - Connect `10k` between bottom two pads of the NMOS (between Gate and Source)
   - Solder one pad od the `330R` to `P1-7`
   - Use thin wire to connect the second pad of the `330R` and gate of the `NMOS``
   - Connect black wire of the buzzer to the upper pad of the `NMOS` (drain). **NOTE** Watchout for the wire! If you pull the wire, you can damage the transistor!
   - Connect red wire of the buzzer to `3V3` pad on the Inkplate

### Optional - Addind an external antenna
If the reception of the WiFi signal is bad, you can boost it by adding an external WiFi antenna. But this requires some serious SMD soldering skills.
1. Remove the old solder from the solder pads for the IPX on the ESP32 and alternate resistor pad.
2. Move ESP32 Antenna 0R resistor to the alternate position. Best advice is to have very steady hands and very pointy soldering tip along with good SMD tweezers.
3. Solder IPX connector. Watch-out for the 