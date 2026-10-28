# QR701 ESPHome External Component

An ESPHome external component for the QR701 58 mm TTL thermal receipt printer.
It writes one block of text to the printer and feeds three lines afterwards,
leaving the receipt ready to tear.

## Hardware

Use the **TTL UART** model of the QR701. Connect the ESP's TX pin to the
printer RX pin and connect ground to ground. The printer is labelled **5–9 V,
2 A**. Use a regulated 9 V supply rated
for at least 2 A (3 A is preferable) and do not power the printer from the ESP
board's GPIO, 3.3 V rail, VIN, or 5 V pin.

Confirm the TTL signal voltage in the printer documentation before connecting
it to a 3.3 V ESP. If the printer's TX output is 5 V, do not connect it to an
ESP RX pin without level shifting. This component can transmit text and ESC/POS
print commands, so the
safe minimal wiring is ESP TX → printer RX plus common ground; omit `rx_pin`
and leave the printer TX wire disconnected. If the printer does not accept the
ESP's 3.3 V TX signal, use an appropriate unidirectional level shifter.

For the documented five-pin QR701 TTL connector, the functions are `VH`,
`DTR/DSR`, `TXD`, `RXD`, and `GND`. Connect only `VH` to the printer supply,
`GND` to both the supply negative and ESP ground, and `RXD` to ESP TX. Leave
`DTR/DSR` and `TXD` disconnected for this transmit-only component. Always
verify the markings on the printer PCB and harness first: connector orientation
and wire colours vary between revisions.

It uses 57 × 30 mm paper, has a 48 mm effective print width (384 dots per
line), and advertises ESC/POS support. The component's direct text-plus-line-
feed output is compatible with that command set, so no protocol change is
needed for this TTL model.

The product listing also offers RS232 and USB variants. RS232 requires a
proper RS232 level shifter, while USB models require a supported ESPHome USB
host UART setup; neither connects directly to GPIO UART pins.

## Install and configure

For a Git-hosted copy of this repository, use this in the device YAML:

```yaml
external_components:
  - source: github://OWNER/REPOSITORY
    components: [qr701]

uart:
  id: printer_uart
  tx_pin: GPIO17
  # For status reporting, connect printer TXD here only after confirming it is
  # safe for an ESP's 3.3 V input (or add a level shifter).
  rx_pin: GPIO16
  baud_rate: 19200  # QR701 TTL documented default; try 9600 as fallback.
  data_bits: 8
  parity: NONE
  stop_bits: 1

qr701:
  id: receipt_printer
  uart_id: printer_uart
```

For local development, replace the Git source with the `external_components`
block in [example.yaml](example.yaml).

## Print, QR, barcode, and feed actions

`qr701.print_text` takes either a text value directly or an object with `id`
and `text`. The text may contain line breaks and may be templated. It wraps at
word boundaries to the QR701's 384-dot print width (32 characters in its
standard 12-dot font). Explicit newlines are kept. A word longer than the
printable width is split so it cannot run off the paper.
`qr701.print` remains a compatible alias.

### Print custom text from Home Assistant

The component automatically creates a writable Home Assistant **Text** entity
and two matching **Buttons** from its `id`. For example, `id: receipt_printer`
creates `Receipt Printer Print Text`, `Receipt Printer Print`, and `Receipt
Printer Print Markdown`. Both buttons print the same saved text-field value:
**Print** sends it literally, while **Print Markdown** renders a practical
Markdown subset (headings, bold, italic, underline-style strikethrough, inline
code, lists, block quotes, links, and horizontal rules). The draft text remains
available for repeat prints or editing.

```yaml
sequence:
  - action: text.set_value
    target:
      entity_id: text.qr701_printer_receipt_printer_print_text
    data:
      value: |-
        Order #1042
        Thank you!
  - action: button.press
    target:
      entity_id: button.qr701_printer_receipt_printer_print
```

Home Assistant assigns the actual entity ID, so use the one shown in your
device's entity list. The text field accepts up to 1,024 characters. Newlines
are retained; non-ASCII characters depend on the printer's ESC/POS code page.

Set `print_text`, `print_button`, or `markdown_print_button` only to override
generated labels or entity settings:

```yaml
qr701:
  id: receipt_printer
  uart_id: printer_uart
  print_text:
    name: Kitchen receipt printer
  print_button:
    name: Print kitchen receipt
  markdown_print_button:
    name: Print kitchen receipt as Markdown
```

```yaml
on_...:
  then:
    - qr701.print_text:
        id: receipt_printer
        text: |-
          Order #42
          Thank you!
```

The component wraps printable text before sending it, then sends three LF bytes
for line feed. The printer needs a compatible text code page for non-ASCII
characters; ASCII is the portable choice unless the unit's manual documents a
different code page.

### Markdown receipts

The Home Assistant text field is rendered as Markdown only when its **Print
Markdown** button is pressed. The normal **Print** button sends it literally.
The `qr701.print_markdown` action with only an `id` does the same thing as the
Markdown button, so it can be used in another ESPHome template button. When
supplied with `text`, it prints that Markdown immediately. `qr701.print_text`
remains literal for backward compatibility.

```yaml
on_...:
  then:
    # Print the current Home Assistant text-field draft.
    - qr701.print_markdown:
        id: receipt_printer

    # Or print Markdown supplied directly by an automation.
    - qr701.print_markdown:
        id: receipt_printer
        text: |-
          # Order #42
          **Paid** - _Thank you!_

          - Espresso
          - Biscotti

          [View order](https://example.com/orders/42)
          ---
```

Markdown is intentionally text-only: images, tables, HTML, and advanced CSS
are not rendered on this receipt printer. Markdown paragraphs, lists, and
quotes wrap at word boundaries while keeping their prefixes. H1 headings use
the printer's double-width font, so they wrap at 16 characters; H2 and H3 use
normal width with double height and retain the normal 32-character width.
Long unbroken words and URLs are split only when necessary to stay within the
384-dot print area.

`qr701.feed` feeds from 0 to 255 lines using the printer's `ESC d n` command.
`qr701.refresh_status` immediately requests a fresh set of status bytes rather
than waiting for the next polling interval.

```yaml
on_...:
  then:
    - qr701.feed:
        id: receipt_printer
        lines: 3
    - qr701.refresh_status:
        id: receipt_printer
```

### QR codes

`qr701.print_qr` uses the printer's native ESC/POS QR-code generator (the
`GS ( k` command family). It selects QR Model 2, stores the supplied text in
the printer, prints it centered, and feeds three lines. The action accepts 1
to 7,092 bytes. `size` is the QR module width from 1 to 16 (default `5`) and
`error_correction` is `L`, `M`, `Q`, or `H` (default `M`). Larger module sizes
are easier to scan but may not fit 58 mm paper for long URLs.

```yaml
on_...:
  then:
    - qr701.print_qr:
        id: receipt_printer
        text: "https://blog.aidanmaurinjones.com"
        size: 5
        error_correction: M
```

The component also creates four Home Assistant QR entities from its id:
`QR Data`, `QR Size`, `QR Error Correction`, and `Print QR`. Set the text
entity to a URL or other payload, choose a module size and `L`, `M`, `Q`, or
`H` correction, then press the button. Their defaults are size `5` and
correction `M`. Set `qr_data`, `qr_size`, `qr_error_correction`, or
`print_qr_button` under `qr701:` to override their generated labels or entity
settings.

### Barcodes

`qr701.print_barcode` uses ESC/POS `GS k`. Its default is centered Code 128
with HRI text printed below the bars. Supported format names are `UPC_A`,
`UPC_E`, `EAN13`, `EAN8`, `CODE39`, `ITF`, `CODABAR`, `CODE93`, and `CODE128`.
For Code 128, ordinary text is automatically encoded using Code Set B; pass a
value beginning with `{A`, `{B`, or `{C` to select a code set yourself.

```yaml
on_...:
  then:
    - qr701.print_barcode:
        id: receipt_printer
        text: "123456789012"
        format: CODE128
        height: 80
        width: 2
        show_text: true
```

`height` ranges from 1 to 255 dots and `width` from 2 to 6. Barcode data is
limited to 252 bytes. The automatically created `Barcode Data` and `Print
Barcode` entities use `CODE128`, height `80`, width `2`, and visible HRI text;
use the action when a different format or appearance is needed. Set
`barcode_data` or `print_barcode_button` under `qr701:` to override their
generated labels or entity settings.

### Firmware compatibility

QR and barcode output requires the printer firmware to implement these native
ESC/POS commands. The QR701 advertises ESC/POS compatibility, but low-cost
printer revisions sometimes implement text commands only. Start with a short
Code 128 value and a short QR URL. If the printer feeds blank paper, garbles
the command, or ignores it, that firmware does not support the corresponding
native feature. This component does not yet rasterize QR codes as a fallback;
that would need a separately verified graphics-command path for the specific
printer revision.

## First test

First power the printer by itself and verify the feed button works. Then wire
only ESP TX → printer RXD and a common ground, configure 19,200 baud / 8-N-1,
and use the example's button. If it prints nothing or garbage, verify the power
and wire direction, then try 9,600 baud. Do not connect printer TX to an ESP
GPIO until its idle voltage has been measured and confirmed safe for 3.3 V
logic (or level-shifted).

## Bidirectional status

Add `rx_pin` to enable two-way communication. The component automatically
creates a `QR701 status` text sensor and `QR701 paper out`, `QR701 cover open`,
and `QR701 error` binary sensors. It polls ESC/POS `DLE EOT` real-time status
commands once per `update_interval` (one second by default), publishing one of
`idle`, `printing`, `offline`, `cover_open`, `paper_out`, `error`, or
`unavailable`.

`paper_out`, `cover_open`, `error`, and `offline` are decoded from printer
replies. ESC/POS does not define a portable, reliable "mechanism currently
printing" bit, so `printing` is a conservative local estimate after submitting
a receipt; it changes to `idle` after the printer next confirms a healthy
status. `unavailable` means the printer did not respond within 100 ms—check
the RX/TX crossing, common ground, baud rate, and logic level.

Set any of `status`, `paper_out`, `cover_open`, or `error` only when you want
to override its name or entity settings. The QR701 has a tear bar, not an
automatic cutter, so this component deliberately has no `cut` action.

## Notes

This component intentionally does not reset the printer or cut paper. Those
commands vary between firmware variants; QR and barcode commands are standard
ESC/POS extensions but still require firmware support as described above.
