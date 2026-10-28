#include "qr701.h"

#include <cctype>
#include <vector>

#include "esphome/core/log.h"

namespace esphome::qr701 {

static const char *const TAG = "qr701";

namespace {

// The QR701 has a 384-dot print area. Font A is 12 dots wide, so normal text
// fits 32 characters; the double-width H1 font fits 16. Keep these as dot
// measurements rather than magic character counts so the printer geometry is
// clear at each call site.
static constexpr size_t PRINTABLE_WIDTH_DOTS = 384;
static constexpr size_t FONT_A_WIDTH_DOTS = 12;
static constexpr size_t NORMAL_LINE_WIDTH = PRINTABLE_WIDTH_DOTS / FONT_A_WIDTH_DOTS;
static constexpr size_t DOUBLE_WIDTH_LINE_WIDTH = NORMAL_LINE_WIDTH / 2;

void write_inline_markdown(QR701 *printer, const std::string &text);

size_t markdown_width(const std::string &text) {
  size_t width = 0;

  for (size_t index = 0; index < text.size();) {
    if (text[index] == '\\' && index + 1 < text.size()) {
      width++;
      index += 2;
    } else if (index + 1 < text.size() &&
               ((text[index] == '*' && text[index + 1] == '*') ||
                (text[index] == '_' && text[index + 1] == '_') ||
                (text[index] == '~' && text[index + 1] == '~'))) {
      index += 2;
    } else if (text[index] == '*' || text[index] == '_' || text[index] == '`') {
      index++;
    } else if (text[index] == '[') {
      const size_t close_label = text.find("](", index + 1);
      const size_t close_url = close_label == std::string::npos ? std::string::npos : text.find(')', close_label + 2);
      if (close_url != std::string::npos) {
        width += markdown_width(text.substr(index + 1, close_label - index - 1));
        // The renderer prints a space and parentheses around the URL.
        width += 3 + close_url - close_label - 2;
        index = close_url + 1;
      } else {
        width++;
        index++;
      }
    } else {
      width++;
      index++;
    }
  }

  return width;
}

size_t text_width(const std::string &text, bool markdown) {
  return markdown ? markdown_width(text) : text.size();
}

std::vector<std::string> wrap_line(const std::string &line, size_t max_width, bool markdown) {
  std::vector<std::string> wrapped;
  std::string current;
  bool pending_space = false;
  size_t index = 0;

  while (index < line.size()) {
    while (index < line.size() && std::isspace(static_cast<unsigned char>(line[index]))) {
      if (!current.empty())
        pending_space = true;
      index++;
    }
    if (index == line.size())
      break;
    const size_t word_start = index;
    if (markdown && line[index] == '[') {
      // A Markdown link label may contain spaces; keep the entire link as one
      // wrap candidate so the inline renderer still recognizes it.
      const size_t close_label = line.find("](", index + 1);
      const size_t close_url = close_label == std::string::npos ? std::string::npos : line.find(')', close_label + 2);
      if (close_url != std::string::npos)
        index = close_url + 1;
    }
    if (index == word_start) {
      while (index < line.size() && !std::isspace(static_cast<unsigned char>(line[index])))
        index++;
    }
    if (word_start == index)
      continue;

    const std::string word = line.substr(word_start, index - word_start);
    const size_t word_width = text_width(word, markdown);
    const size_t separator_width = pending_space && !current.empty() ? 1 : 0;
    if (!current.empty() && text_width(current, markdown) + separator_width + word_width <= max_width) {
      if (separator_width != 0)
        current += ' ';
      current += word;
      pending_space = false;
      continue;
    }

    if (!current.empty()) {
      wrapped.push_back(current);
      current.clear();
    }
    pending_space = false;

    if (word_width <= max_width) {
      current = word;
      continue;
    }

    // There is no word boundary available. Split conservatively by source
    // bytes so a word (or a long URL) cannot overflow the receipt width.
    // Markdown punctuation can only reduce the rendered width, so this is safe.
    for (size_t offset = 0; offset < word.size(); offset += max_width)
      wrapped.push_back(word.substr(offset, max_width));
  }

  if (!current.empty())
    wrapped.push_back(current);
  return wrapped;
}

uint32_t write_wrapped_text(QR701 *printer, const std::string &text, size_t max_width, bool markdown) {
  if (text.empty())
    return 0;

  uint32_t line_count = 0;
  size_t line_start = 0;
  while (line_start < text.size()) {
    const size_t line_end = text.find('\n', line_start);
    std::string line = text.substr(line_start, line_end - line_start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    const std::vector<std::string> lines = wrap_line(line, max_width, markdown);
    if (lines.empty()) {
      printer->write_byte('\n');
      line_count++;
    } else {
      for (const std::string &wrapped_line : lines) {
        if (markdown)
          write_inline_markdown(printer, wrapped_line);
        else
          printer->write_array(reinterpret_cast<const uint8_t *>(wrapped_line.data()), wrapped_line.size());
        printer->write_byte('\n');
        line_count++;
      }
    }

    if (line_end == std::string::npos)
      break;
    line_start = line_end + 1;
  }
  return line_count;
}

void write_escape(QR701 *printer, uint8_t command, uint8_t value) {
  printer->write_byte(0x1B);
  printer->write_byte(command);
  printer->write_byte(value);
}

void write_italic(QR701 *printer, bool enabled) {
  // ESC 4 enables italic and ESC 5 disables it; unlike ESC E and ESC -, these
  // commands do not take a parameter byte.
  printer->write_byte(0x1B);
  printer->write_byte(enabled ? 0x34 : 0x35);
}

void write_inline_markdown(QR701 *printer, const std::string &text) {
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool code = false;

  for (size_t index = 0; index < text.size();) {
    if (text[index] == '\\' && index + 1 < text.size()) {
      printer->write_byte(static_cast<uint8_t>(text[index + 1]));
      index += 2;
    } else if (index + 1 < text.size() &&
               ((text[index] == '*' && text[index + 1] == '*') ||
                (text[index] == '_' && text[index + 1] == '_'))) {
      bold = !bold;
      write_escape(printer, 0x45, bold ? 1 : 0);  // ESC E: emphasis.
      index += 2;
    } else if (index + 1 < text.size() && text[index] == '~' && text[index + 1] == '~') {
      // ESC/POS has no portable strikethrough; underline is the closest
      // broadly supported receipt-printer equivalent.
      underline = !underline;
      write_escape(printer, 0x2D, underline ? 1 : 0);  // ESC -: underline.
      index += 2;
    } else if (text[index] == '*' || text[index] == '_') {
      italic = !italic;
      write_italic(printer, italic);
      index++;
    } else if (text[index] == '`') {
      code = !code;
      write_escape(printer, 0x4D, code ? 1 : 0);  // ESC M: Font B / Font A.
      index++;
    } else if (text[index] == '[') {
      const size_t close_label = text.find("](", index + 1);
      const size_t close_url = close_label == std::string::npos ? std::string::npos : text.find(')', close_label + 2);
      if (close_url != std::string::npos) {
        write_inline_markdown(printer, text.substr(index + 1, close_label - index - 1));
        const std::string url = text.substr(close_label + 2, close_url - close_label - 2);
        printer->write_byte(' ');
        printer->write_byte('(');
        printer->write_array(reinterpret_cast<const uint8_t *>(url.data()), url.size());
        printer->write_byte(')');
        index = close_url + 1;
      } else {
        printer->write_byte(static_cast<uint8_t>(text[index++]));
      }
    } else {
      printer->write_byte(static_cast<uint8_t>(text[index++]));
    }
  }

  // Do not allow an incomplete Markdown marker to leak formatting into the
  // following line or a later print job.
  write_escape(printer, 0x45, 0);
  write_italic(printer, false);
  write_escape(printer, 0x2D, 0);
  write_escape(printer, 0x4D, 0);
}

bool is_horizontal_rule(const std::string &line) {
  if (line.size() < 3)
    return false;
  const char marker = line[0];
  if (marker != '-' && marker != '*' && marker != '_')
    return false;
  for (const char character : line) {
    if (character != marker)
      return false;
  }
  return true;
}

}  // namespace

void QR701::print_text_field() {
  if (this->print_text_ == nullptr) {
    ESP_LOGW(TAG, "Print button pressed without a text entity");
    return;
  }
  this->print(this->print_text_->state);
}

void QR701::print_markdown_text_field() {
  if (this->print_text_ == nullptr) {
    ESP_LOGW(TAG, "Print Markdown button pressed without a text entity");
    return;
  }
  this->print_markdown(this->print_text_->state);
}

void QR701::print_qr_data_field() {
  if (this->qr_data_ == nullptr) {
    ESP_LOGW(TAG, "Print QR button pressed without a QR data entity");
    return;
  }
  uint8_t size = 5;
  if (this->qr_size_ != nullptr && this->qr_size_->has_state())
    size = static_cast<uint8_t>(this->qr_size_->state);

  uint8_t error_correction = 49;  // ESC/POS M.
  if (this->qr_error_correction_ != nullptr) {
    const StringRef selected = this->qr_error_correction_->current_option();
    if (selected == "L")
      error_correction = 48;
    else if (selected == "Q")
      error_correction = 50;
    else if (selected == "H")
      error_correction = 51;
  }
  this->print_qr(this->qr_data_->state, size, error_correction);
}

void QR701::print_barcode_data_field() {
  if (this->barcode_data_ == nullptr) {
    ESP_LOGW(TAG, "Print Barcode button pressed without a barcode data entity");
    return;
  }
  // Code 128 is the most flexible widely supported ESC/POS barcode type.
  this->print_barcode(this->barcode_data_->state, 73, 80, 2, true);
}

void QR701::start_print_(const std::string &text) {
  const uint32_t line_count = write_wrapped_text(this, text, NORMAL_LINE_WIDTH, false);

  // Terminate the final text line and advance the receipt far enough to tear.
  static constexpr uint8_t FEED[] = {'\n', '\n', '\n'};
  this->write_array(FEED, sizeof(FEED));

  // ESC/POS status has no portable "mechanism currently printing" bit.
  // Keep a local state briefly, then verify the printer's fault state.
  this->printing_until_ = millis() + 250 + (line_count == 0 ? 1 : line_count) * 100;
  this->printing_ = true;
  this->publish_status_("printing");
}

void QR701::start_qr_print_(const std::string &text, uint8_t size, uint8_t error_correction) {
  // ESC/POS QR commands use GS ( k. Model 2 is broadly supported by modern
  // thermal printers; its command data length includes cn, fn, and m/data.
  if (text.empty() || text.size() > 7092) {
    ESP_LOGW(TAG, "QR data must contain 1 to 7092 bytes; refusing to print %u bytes",
             static_cast<unsigned>(text.size()));
    this->publish_status_("error");
    return;
  }
  if (size < 1 || size > 16 || error_correction < 48 || error_correction > 51) {
    ESP_LOGW(TAG, "Invalid QR options (size=%u, error correction=%u)", size, error_correction);
    this->publish_status_("error");
    return;
  }

  auto write_qr_command = [this](uint16_t length, uint8_t cn, uint8_t fn, uint8_t parameter) {
    this->write_byte(0x1D);  // GS
    this->write_byte(0x28);  // (
    this->write_byte(0x6B);  // k
    this->write_byte(length & 0xFF);
    this->write_byte(length >> 8);
    this->write_byte(cn);
    this->write_byte(fn);
    this->write_byte(parameter);
  };

  this->write_byte(0x1D);                          // GS ( k: select QR Model 2.
  this->write_byte(0x28);
  this->write_byte(0x6B);
  this->write_byte(4);
  this->write_byte(0);
  this->write_byte(49);
  this->write_byte(65);
  this->write_byte(50);
  this->write_byte(0);
  write_qr_command(3, 49, 67, size);               // Set module size.
  write_qr_command(3, 49, 69, error_correction);   // Set error correction.
  const uint16_t store_length = static_cast<uint16_t>(text.size() + 3);
  this->write_byte(0x1D);
  this->write_byte(0x28);
  this->write_byte(0x6B);
  this->write_byte(store_length & 0xFF);
  this->write_byte(store_length >> 8);
  this->write_byte(49);
  this->write_byte(80);
  this->write_byte(48);
  this->write_array(reinterpret_cast<const uint8_t *>(text.data()), text.size());
  write_qr_command(3, 49, 81, 48);  // Print the stored symbol.

  static constexpr uint8_t FEED[] = {'\n', '\n', '\n'};
  this->write_array(FEED, sizeof(FEED));
  this->printing_until_ = millis() + 750 + static_cast<uint32_t>(text.size()) * 4;
  this->printing_ = true;
  this->publish_status_("printing");
}

void QR701::start_barcode_print_(const std::string &text, uint8_t format, uint8_t height, uint8_t width,
                                 bool show_text) {
  if (text.empty() || text.size() > 252) {
    ESP_LOGW(TAG, "Barcode data must contain 1 to 252 bytes; refusing to print %u bytes",
             static_cast<unsigned>(text.size()));
    this->publish_status_("error");
    return;
  }
  if (width < 2 || width > 6) {
    ESP_LOGW(TAG, "Barcode width must be from 2 to 6");
    this->publish_status_("error");
    return;
  }

  // GS H/h/w configure HRI text, barcode height, and narrow-bar width.
  this->write_byte(0x1D);
  this->write_byte(0x48);
  this->write_byte(show_text ? 2 : 0);  // HRI below the bars, or hidden.
  this->write_byte(0x1D);
  this->write_byte(0x68);
  this->write_byte(height);
  this->write_byte(0x1D);
  this->write_byte(0x77);
  this->write_byte(width);
  this->write_byte(0x1B);
  this->write_byte(0x61);
  this->write_byte(1);  // Centre the symbol.

  this->write_byte(0x1D);
  this->write_byte(0x6B);
  this->write_byte(format);
  if (format >= 65) {
    // GS k m n d1...dn (Function B). Prefix Code 128 data with Code Set B
    // unless the caller deliberately supplies an ESC/POS code-set selector.
    const bool has_code_set = format == 73 && text.size() >= 2 && text[0] == '{' &&
                              (text[1] == 'A' || text[1] == 'B' || text[1] == 'C');
    const uint8_t prefix_length = format == 73 && !has_code_set ? 2 : 0;
    const uint8_t length = static_cast<uint8_t>(text.size() + prefix_length);
    this->write_byte(length);
    if (prefix_length != 0) {
      this->write_byte('{');
      this->write_byte('B');
    }
    this->write_array(reinterpret_cast<const uint8_t *>(text.data()), text.size());
  } else {
    // GS k m d1...dk NUL (Function A) for UPC/EAN, Code 39, ITF, and
    // Codabar. The printer validates symbology-specific data itself.
    this->write_array(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    this->write_byte(0);
  }
  this->write_byte('\n');
  this->write_byte(0x1B);
  this->write_byte(0x61);
  this->write_byte(0);  // Restore left alignment for subsequent receipts.
  static constexpr uint8_t FEED[] = {'\n', '\n'};
  this->write_array(FEED, sizeof(FEED));
  this->printing_until_ = millis() + 500 + static_cast<uint32_t>(height) * 8;
  this->printing_ = true;
  this->publish_status_("printing");
}

void QR701::run_queued_job_() {
  const QueuedJob job = this->queued_job_;
  if (job == QueuedJob::NONE)
    return;
  this->queued_job_ = QueuedJob::NONE;

  switch (job) {
    case QueuedJob::TEXT:
      this->start_print_(this->queued_text_);
      break;
    case QueuedJob::MARKDOWN:
      this->start_markdown_print_(this->queued_text_);
      break;
    case QueuedJob::FEED:
      this->start_feed_(this->queued_feed_lines_);
      break;
    case QueuedJob::QR:
      this->start_qr_print_(this->queued_text_, this->queued_qr_size_, this->queued_qr_error_correction_);
      break;
    case QueuedJob::BARCODE:
      this->start_barcode_print_(this->queued_text_, this->queued_barcode_format_, this->queued_barcode_height_,
                                 this->queued_barcode_width_, this->queued_barcode_show_text_);
      break;
    case QueuedJob::NONE:
      break;
  }
  this->queued_text_.clear();
}

void QR701::start_markdown_print_(const std::string &markdown) {
  size_t line_start = 0;
  uint32_t line_count = 0;

  // An empty Markdown document still prints one blank line, matching the
  // component's previous behaviour.
  if (markdown.empty()) {
    this->write_byte('\n');
    line_count++;
  }

  while (line_start < markdown.size()) {
    const size_t line_end = markdown.find('\n', line_start);
    std::string line = markdown.substr(line_start, line_end - line_start);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (is_horizontal_rule(line)) {
      static constexpr char RULE[] = "--------------------------------";
      this->write_array(reinterpret_cast<const uint8_t *>(RULE), sizeof(RULE) - 1);
      this->write_byte('\n');
      line_count++;
    } else {
      size_t content_start = 0;
      uint8_t heading_level = 0;
      while (content_start < line.size() && line[content_start] == '#' && heading_level < 3) {
        heading_level++;
        content_start++;
      }
      if (heading_level > 0 && content_start < line.size() && line[content_start] == ' ') {
        while (content_start < line.size() && line[content_start] == ' ')
          content_start++;
        const size_t heading_width = heading_level == 1 ? DOUBLE_WIDTH_LINE_WIDTH : NORMAL_LINE_WIDTH;
        const std::vector<std::string> lines = wrap_line(line.substr(content_start), heading_width, true);
        for (const std::string &wrapped_line : lines) {
          write_escape(this, 0x61, 1);  // ESC a: centre headings.
          write_escape(this, 0x21, heading_level == 1 ? 0x30 : 0x10);
          write_escape(this, 0x45, 1);
          write_inline_markdown(this, wrapped_line);
          write_escape(this, 0x21, 0);
          write_escape(this, 0x61, 0);
          this->write_byte('\n');
          line_count++;
        }
      } else if (line.rfind("> ", 0) == 0) {
        const std::vector<std::string> lines = wrap_line(line.substr(2), NORMAL_LINE_WIDTH - 2, true);
        for (const std::string &wrapped_line : lines) {
          this->write_array(reinterpret_cast<const uint8_t *>("| "), 2);
          write_inline_markdown(this, wrapped_line);
          this->write_byte('\n');
          line_count++;
        }
      } else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0 || line.rfind("+ ", 0) == 0) {
        const std::vector<std::string> lines = wrap_line(line.substr(2), NORMAL_LINE_WIDTH - 2, true);
        for (const std::string &wrapped_line : lines) {
          this->write_array(reinterpret_cast<const uint8_t *>("- "), 2);
          write_inline_markdown(this, wrapped_line);
          this->write_byte('\n');
          line_count++;
        }
      } else {
        const std::vector<std::string> lines = wrap_line(line, NORMAL_LINE_WIDTH, true);
        if (lines.empty()) {
          this->write_byte('\n');
          line_count++;
        } else {
          for (const std::string &wrapped_line : lines) {
            write_inline_markdown(this, wrapped_line);
            this->write_byte('\n');
            line_count++;
          }
        }
      }
    }
    if (line_end == std::string::npos)
      break;
    line_start = line_end + 1;
  }

  static constexpr uint8_t FEED[] = {'\n', '\n', '\n'};
  this->write_array(FEED, sizeof(FEED));
  this->printing_until_ = millis() + 250 + line_count * 100;
  this->printing_ = true;
  this->publish_status_("printing");
}

void QR701::setup() {
  // Number and select entities are not Components, so the parent provides
  // their useful defaults when ESPHome starts.
  if (this->qr_size_ != nullptr)
    this->qr_size_->publish_state(5);
  if (this->qr_error_correction_ != nullptr)
    this->qr_error_correction_->publish_state("M");
}

void QR701::update() {
  if (!this->status_enabled_() || this->awaiting_status_)
    return;
  this->refresh_status();
}

void QR701::refresh_status() {
  if (this->awaiting_status_)
    return;
  this->offline_ = false;
  this->cover_open_ = false;
  this->paper_out_ = false;
  this->error_ = false;
  this->request_status_(1);
}

void QR701::loop() {
  if (!this->awaiting_status_)
    return;

  uint8_t response;
  if (this->read_byte(&response)) {
    this->awaiting_status_ = false;
    this->process_status_(response);
    if (this->query_ < 4) {
      this->request_status_(this->query_ + 1);
    } else {
      this->publish_printer_status_();
      this->run_queued_job_();
    }
    return;
  }

  if (millis() - this->query_started_at_ > 100) {
    this->awaiting_status_ = false;
    this->publish_status_("unavailable");
    this->run_queued_job_();
  }
}

void QR701::request_status_(uint8_t query) {
  this->query_ = query;
  this->write_byte(0x10);  // DLE
  this->write_byte(0x04);  // EOT
  this->write_byte(query);
  this->query_started_at_ = millis();
  this->awaiting_status_ = true;
}

void QR701::process_status_(uint8_t status) {
  // ESC/POS DLE EOT n real-time responses, n = 1..4.
  switch (this->query_) {
    case 1:
      this->offline_ = (status & 0x08) != 0;
      break;
    case 2:
      this->cover_open_ = (status & 0x04) != 0;
      this->paper_out_ = (status & 0x20) != 0;
      this->error_ = (status & 0x40) != 0;
      break;
    case 3:
      // DLE EOT 3: bits 2, 3, 5, and 6 report recoverable, cutter,
      // unrecoverable, and auto-recoverable errors respectively.
      this->error_ = this->error_ || (status & 0x6C) != 0;
      break;
    case 4:
      this->paper_out_ = this->paper_out_ || (status & 0x60) != 0;
      break;
  }
}

void QR701::publish_printer_status_() {
  this->publish_binary_status_();
  if (this->error_) {
    this->publish_status_("error");
  } else if (this->paper_out_) {
    this->publish_status_("paper_out");
  } else if (this->cover_open_) {
    this->publish_status_("cover_open");
  } else if (this->offline_) {
    this->publish_status_("offline");
  } else if (this->printing_ && millis() < this->printing_until_) {
    this->publish_status_("printing");
  } else {
    this->printing_ = false;
    this->publish_status_("idle");
  }
}

void QR701::publish_binary_status_() {
  if (this->paper_out_sensor_ != nullptr)
    this->paper_out_sensor_->publish_state(this->paper_out_);
  if (this->cover_open_sensor_ != nullptr)
    this->cover_open_sensor_->publish_state(this->cover_open_);
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(this->error_);
}

void QR701::publish_status_(const char *status) {
  ESP_LOGD(TAG, "Printer status: %s", status);
  if (this->status_ != nullptr)
    this->status_->publish_state(status);
}

bool QR701::status_enabled_() const {
  return this->status_ != nullptr || this->paper_out_sensor_ != nullptr || this->cover_open_sensor_ != nullptr ||
         this->error_sensor_ != nullptr;
}

void QR701::dump_config() {
  ESP_LOGCONFIG(TAG, "QR701 thermal receipt printer");
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace esphome::qr701
