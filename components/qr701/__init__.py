"""ESPHome support for the QR701 TTL thermal receipt printer."""

import inspect

import esphome.automation as automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.components import button
from esphome.components import text
from esphome.components import text_sensor
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "button", "text", "text_sensor"]

CONF_TEXT = "text"
CONF_PRINT_TEXT = "print_text"
CONF_PRINT_BUTTON = "print_button"
CONF_MARKDOWN_PRINT_BUTTON = "markdown_print_button"
CONF_STATUS = "status"
CONF_PAPER_OUT = "paper_out"
CONF_COVER_OPEN = "cover_open"
CONF_ERROR = "error"
CONF_LINES = "lines"

DEFAULT_STATUS_CONFIG = {"name": "QR701 status"}
DEFAULT_PAPER_OUT_CONFIG = {"name": "QR701 paper out"}
DEFAULT_COVER_OPEN_CONFIG = {"name": "QR701 cover open"}
DEFAULT_ERROR_CONFIG = {"name": "QR701 error"}

qr701_ns = cg.esphome_ns.namespace("qr701")
QR701 = qr701_ns.class_("QR701", cg.PollingComponent, uart.UARTDevice)
QR701PrintText = qr701_ns.class_("QR701PrintText", text.Text)
QR701PrintButton = qr701_ns.class_("QR701PrintButton", button.Button)
QR701MarkdownPrintButton = qr701_ns.class_("QR701MarkdownPrintButton", button.Button)
QR701PrintAction = qr701_ns.class_("QR701PrintAction", automation.Action)
QR701MarkdownPrintAction = qr701_ns.class_("QR701MarkdownPrintAction", automation.Action)
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
    return config


CONFIG_SCHEMA = cv.All(
    _add_default_print_text,
    cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(QR701),
        cv.Optional(CONF_PRINT_TEXT): text.text_schema(QR701PrintText, mode="TEXT"),
        cv.Optional(CONF_PRINT_BUTTON): button.button_schema(QR701PrintButton),
        cv.Optional(CONF_MARKDOWN_PRINT_BUTTON): button.button_schema(QR701MarkdownPrintButton),
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
