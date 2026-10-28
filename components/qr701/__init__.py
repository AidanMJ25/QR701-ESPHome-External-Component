"""ESPHome support for the QR701 TTL thermal receipt printer."""

import inspect

import esphome.automation as automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import button
from esphome.components import number
from esphome.components import select
from esphome.components import text
from esphome.components import text_sensor
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "button", "number", "select", "text", "text_sensor"]

CONF_TEXT = "text"
CONF_PRINT_TEXT = "print_text"
CONF_PRINT_BUTTON = "print_button"
CONF_MARKDOWN_PRINT_BUTTON = "markdown_print_button"
CONF_QR_DATA = "qr_data"
CONF_QR_SIZE = "qr_size"
CONF_QR_ERROR_CORRECTION = "qr_error_correction"
CONF_PRINT_QR_BUTTON = "print_qr_button"
CONF_BARCODE_DATA = "barcode_data"
CONF_PRINT_BARCODE_BUTTON = "print_barcode_button"
CONF_STATUS = "status"
CONF_PAPER_OUT = "paper_out"
CONF_COVER_OPEN = "cover_open"
CONF_ERROR = "error"
CONF_LINES = "lines"
CONF_SIZE = "size"
CONF_ERROR_CORRECTION = "error_correction"
CONF_FORMAT = "format"
CONF_HEIGHT = "height"
CONF_WIDTH = "width"
CONF_SHOW_TEXT = "show_text"

DEFAULT_STATUS_CONFIG = {"name": "QR701 status"}
DEFAULT_PAPER_OUT_CONFIG = {"name": "QR701 paper out"}
DEFAULT_COVER_OPEN_CONFIG = {"name": "QR701 cover open"}
DEFAULT_ERROR_CONFIG = {"name": "QR701 error"}

qr701_ns = cg.esphome_ns.namespace("qr701")
QR701 = qr701_ns.class_("QR701", cg.PollingComponent, uart.UARTDevice)
QR701PrintText = qr701_ns.class_("QR701PrintText", text.Text)
QR701PrintButton = qr701_ns.class_("QR701PrintButton", button.Button)
QR701MarkdownPrintButton = qr701_ns.class_("QR701MarkdownPrintButton", button.Button)
QR701QRData = qr701_ns.class_("QR701QRData", text.Text)
QR701QRSize = qr701_ns.class_("QR701QRSize", number.Number)
QR701QRErrorCorrection = qr701_ns.class_("QR701QRErrorCorrection", select.Select)
QR701PrintQRButton = qr701_ns.class_("QR701PrintQRButton", button.Button)
QR701BarcodeData = qr701_ns.class_("QR701BarcodeData", text.Text)
QR701PrintBarcodeButton = qr701_ns.class_("QR701PrintBarcodeButton", button.Button)
QR701PrintAction = qr701_ns.class_("QR701PrintAction", automation.Action)
QR701MarkdownPrintAction = qr701_ns.class_("QR701MarkdownPrintAction", automation.Action)
QR701PrintQRAction = qr701_ns.class_("QR701PrintQRAction", automation.Action)
QR701PrintBarcodeAction = qr701_ns.class_("QR701PrintBarcodeAction", automation.Action)
QR701FeedAction = qr701_ns.class_("QR701FeedAction", automation.Action)
QR701RefreshStatusAction = qr701_ns.class_("QR701RefreshStatusAction", automation.Action)


def _add_default_print_text(config):
    """Create Home Assistant print controls unless the user overrides them."""
    config = config.copy()
    component_id = str(config.get(CONF_ID, "qr701")).replace("_", " ").title()
    if CONF_PRINT_TEXT not in config:
        config[CONF_PRINT_TEXT] = {"name": f"{component_id} Print Text"}
    if CONF_PRINT_BUTTON not in config:
        config[CONF_PRINT_BUTTON] = {"name": f"{component_id} Print"}
    if CONF_MARKDOWN_PRINT_BUTTON not in config:
        config[CONF_MARKDOWN_PRINT_BUTTON] = {"name": f"{component_id} Print Markdown"}
    if CONF_QR_DATA not in config:
        config[CONF_QR_DATA] = {"name": f"{component_id} QR Data"}
    if CONF_QR_SIZE not in config:
        config[CONF_QR_SIZE] = {"name": f"{component_id} QR Size"}
    if CONF_QR_ERROR_CORRECTION not in config:
        config[CONF_QR_ERROR_CORRECTION] = {"name": f"{component_id} QR Error Correction"}
    if CONF_PRINT_QR_BUTTON not in config:
        config[CONF_PRINT_QR_BUTTON] = {"name": f"{component_id} Print QR"}
    if CONF_BARCODE_DATA not in config:
        config[CONF_BARCODE_DATA] = {"name": f"{component_id} Barcode Data"}
    if CONF_PRINT_BARCODE_BUTTON not in config:
        config[CONF_PRINT_BARCODE_BUTTON] = {"name": f"{component_id} Print Barcode"}
    return config


CONFIG_SCHEMA = cv.All(
    _add_default_print_text,
    cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(QR701),
        cv.Optional(CONF_PRINT_TEXT): text.text_schema(QR701PrintText, mode="TEXT"),
        cv.Optional(CONF_PRINT_BUTTON): button.button_schema(QR701PrintButton),
        cv.Optional(CONF_MARKDOWN_PRINT_BUTTON): button.button_schema(QR701MarkdownPrintButton),
        cv.Optional(CONF_QR_DATA): text.text_schema(QR701QRData, mode="TEXT"),
        cv.Optional(CONF_QR_SIZE): number.number_schema(QR701QRSize),
        cv.Optional(CONF_QR_ERROR_CORRECTION): select.select_schema(QR701QRErrorCorrection),
        cv.Optional(CONF_PRINT_QR_BUTTON): button.button_schema(QR701PrintQRButton),
        cv.Optional(CONF_BARCODE_DATA): text.text_schema(QR701BarcodeData, mode="TEXT"),
        cv.Optional(CONF_PRINT_BARCODE_BUTTON): button.button_schema(QR701PrintBarcodeButton),
        cv.Optional(CONF_STATUS, default=DEFAULT_STATUS_CONFIG): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_PAPER_OUT, default=DEFAULT_PAPER_OUT_CONFIG): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_COVER_OPEN, default=DEFAULT_COVER_OPEN_CONFIG): binary_sensor.binary_sensor_schema(),
        cv.Optional(CONF_ERROR, default=DEFAULT_ERROR_CONFIG): binary_sensor.binary_sensor_schema(),
    }
    ).extend(cv.polling_component_schema("1s")).extend(uart.UART_DEVICE_SCHEMA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if print_text_config := config.get(CONF_PRINT_TEXT):
        print_text = await text.new_text(print_text_config, max_length=1024)
        cg.add(var.set_print_text(print_text))
    if print_button_config := config.get(CONF_PRINT_BUTTON):
        print_button = await button.new_button(print_button_config)
        cg.add(print_button.set_parent(var))
    if markdown_print_button_config := config.get(CONF_MARKDOWN_PRINT_BUTTON):
        markdown_print_button = await button.new_button(markdown_print_button_config)
        cg.add(markdown_print_button.set_parent(var))
    if qr_data_config := config.get(CONF_QR_DATA):
        qr_data = await text.new_text(qr_data_config, max_length=1024)
        cg.add(var.set_qr_data(qr_data))
    if qr_size_config := config.get(CONF_QR_SIZE):
        qr_size = await number.new_number(qr_size_config, min_value=1, max_value=16, step=1)
        cg.add(var.set_qr_size(qr_size))
    if qr_error_correction_config := config.get(CONF_QR_ERROR_CORRECTION):
        qr_error_correction = await select.new_select(
            qr_error_correction_config, options=["L", "M", "Q", "H"]
        )
        cg.add(var.set_qr_error_correction(qr_error_correction))
    if print_qr_button_config := config.get(CONF_PRINT_QR_BUTTON):
        print_qr_button = await button.new_button(print_qr_button_config)
        cg.add(print_qr_button.set_parent(var))
    if barcode_data_config := config.get(CONF_BARCODE_DATA):
        barcode_data = await text.new_text(barcode_data_config, max_length=252)
        cg.add(var.set_barcode_data(barcode_data))
    if print_barcode_button_config := config.get(CONF_PRINT_BARCODE_BUTTON):
        print_barcode_button = await button.new_button(print_barcode_button_config)
        cg.add(print_barcode_button.set_parent(var))
    if status_config := config.get(CONF_STATUS):
        status = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_text_sensor(status))
    for key, setter in (
        (CONF_PAPER_OUT, "set_paper_out_binary_sensor"),
        (CONF_COVER_OPEN, "set_cover_open_binary_sensor"),
        (CONF_ERROR, "set_error_binary_sensor"),
    ):
        if sensor_config := config.get(key):
            sensor = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(getattr(var, setter)(sensor))


PRINT_ACTION_SCHEMA = cv.maybe_simple_value(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(QR701),
            cv.Required(CONF_TEXT): cv.templatable(cv.string),
        }
    ),
    key=CONF_TEXT,
)

MARKDOWN_PRINT_ACTION_SCHEMA = cv.Any(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(QR701),
        }
    ),
    cv.maybe_simple_value(
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(QR701),
                cv.Required(CONF_TEXT): cv.templatable(cv.string),
            }
        ),
        key=CONF_TEXT,
    ),
)


# ESPHome 2026.3 requires this explicit flag; keeping the decorator compatible
# with older ESPHome releases makes the external component easier to reuse.
_register_action_kwargs = {}
if "synchronous" in inspect.signature(automation.register_action).parameters:
    _register_action_kwargs["synchronous"] = True


@automation.register_action(
    "qr701.print_text", QR701PrintAction, PRINT_ACTION_SCHEMA, **_register_action_kwargs
)
@automation.register_action(
    "qr701.print", QR701PrintAction, PRINT_ACTION_SCHEMA, **_register_action_kwargs
)
async def qr701_print_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
    cg.add(var.set_text(template_))
    return var


@automation.register_action(
    "qr701.print_markdown",
    QR701MarkdownPrintAction,
    MARKDOWN_PRINT_ACTION_SCHEMA,
    **_register_action_kwargs,
)
async def qr701_print_markdown_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    if CONF_TEXT in config:
        template_ = await cg.templatable(config[CONF_TEXT], args, cg.std_string)
        cg.add(var.set_text(template_))
        cg.add(var.set_has_text(True))
    return var


QR_ERROR_CORRECTION = {"L": 48, "M": 49, "Q": 50, "H": 51}
BARCODE_FORMATS = {
    "UPC_A": 0,
    "UPC_E": 1,
    "EAN13": 2,
    "EAN8": 3,
    "CODE39": 4,
    "ITF": 5,
    "CODABAR": 6,
    "CODE93": 72,
    "CODE128": 73,
}

PRINT_QR_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(QR701),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_SIZE, default=5): cv.templatable(cv.int_range(min=1, max=16)),
        cv.Optional(CONF_ERROR_CORRECTION, default="M"): cv.enum(QR_ERROR_CORRECTION, upper=True),
    }
)


@automation.register_action(
    "qr701.print_qr", QR701PrintQRAction, PRINT_QR_ACTION_SCHEMA, **_register_action_kwargs
)
async def qr701_print_qr_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_text(await cg.templatable(config[CONF_TEXT], args, cg.std_string)))
    cg.add(var.set_size(await cg.templatable(config[CONF_SIZE], args, cg.uint8)))
    cg.add(
        var.set_error_correction(
            await cg.templatable(config[CONF_ERROR_CORRECTION], args, cg.uint8)
        )
    )
    return var


PRINT_BARCODE_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(QR701),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_FORMAT, default="CODE128"): cv.enum(BARCODE_FORMATS, upper=True),
        cv.Optional(CONF_HEIGHT, default=80): cv.templatable(cv.int_range(min=1, max=255)),
        cv.Optional(CONF_WIDTH, default=2): cv.templatable(cv.int_range(min=2, max=6)),
        cv.Optional(CONF_SHOW_TEXT, default=True): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "qr701.print_barcode", QR701PrintBarcodeAction, PRINT_BARCODE_ACTION_SCHEMA, **_register_action_kwargs
)
async def qr701_print_barcode_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_text(await cg.templatable(config[CONF_TEXT], args, cg.std_string)))
    cg.add(var.set_format(await cg.templatable(config[CONF_FORMAT], args, cg.uint8)))
    cg.add(var.set_height(await cg.templatable(config[CONF_HEIGHT], args, cg.uint8)))
    cg.add(var.set_width(await cg.templatable(config[CONF_WIDTH], args, cg.uint8)))
    cg.add(var.set_show_text(await cg.templatable(config[CONF_SHOW_TEXT], args, cg.bool_)))
    return var


FEED_ACTION_SCHEMA = cv.maybe_simple_value(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(QR701),
            cv.Required(CONF_LINES): cv.templatable(cv.int_range(min=0, max=255)),
        }
    ),
    key=CONF_LINES,
)


@automation.register_action(
    "qr701.feed", QR701FeedAction, FEED_ACTION_SCHEMA, **_register_action_kwargs
)
async def qr701_feed_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    template_ = await cg.templatable(config[CONF_LINES], args, cg.uint8)
    cg.add(var.set_lines(template_))
    return var


@automation.register_action(
    "qr701.refresh_status",
    QR701RefreshStatusAction,
    cv.Schema({cv.GenerateID(): cv.use_id(QR701)}),
    **_register_action_kwargs,
)
async def qr701_refresh_status_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
