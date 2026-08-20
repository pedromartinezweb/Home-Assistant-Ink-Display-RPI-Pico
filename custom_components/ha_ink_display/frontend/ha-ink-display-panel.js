const escapeHtml = (value) => String(value ?? "")
  .replaceAll("&", "&amp;")
  .replaceAll("<", "&lt;")
  .replaceAll(">", "&gt;")
  .replaceAll('"', "&quot;");

class HaInkDisplayPanel extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open" });
    this._entries = [];
    this._entryId = "";
    this._layout = null;
    this._selectedId = "";
    this._loaded = false;
    this._saving = false;
    this._notice = "";
  }

  set hass(value) {
    this._hass = value;
    if (!this._loaded) {
      this._loaded = true;
      this._load();
    }
  }

  set narrow(value) {
    this._narrow = value;
  }

  set route(value) {
    this._route = value;
  }

  set panel(value) {
    this._panel = value;
  }

  async _load() {
    try {
      const result = await this._hass.callWS({ type: "ha_ink_display/config" });
      this._entries = result.entries;
      if (this._entries.length) {
        this._selectEntry(this._entries[0].entry_id);
      } else {
        this._renderEmpty();
      }
    } catch (error) {
      this._renderError(error.message || "Unable to load the display configuration.");
    }
  }

  _selectEntry(entryId) {
    const entry = this._entries.find((item) => item.entry_id === entryId);
    if (!entry) return;
    this._entryId = entryId;
    this._layout = structuredClone(entry.layout);
    this._layout.items = this._layout.items.map((item) => ({
      ...item,
      _id: crypto.randomUUID(),
    }));
    this._selectedId = this._layout.items[0]?._id || "";
    this._notice = "";
    this._render();
  }

  _items(row) {
    return this._layout.items.filter((item) => Number(item.row) === row);
  }

  _selected() {
    return this._layout.items.find((item) => item._id === this._selectedId);
  }

  _add(row) {
    if (this._items(row).length >= 4) return;
    const item = {
      _id: crypto.randomUUID(),
      row,
      entity: "",
      label: "",
      unit: "",
      decimals: 0,
      alert_mode: "off",
      alert_threshold: "",
    };
    this._layout.items.push(item);
    this._selectedId = item._id;
    this._notice = "";
    this._render();
  }

  _remove() {
    this._layout.items = this._layout.items.filter(
      (item) => item._id !== this._selectedId,
    );
    this._selectedId = this._layout.items[0]?._id || "";
    this._notice = "";
    this._render();
  }

  _move(id, row, position) {
    const item = this._layout.items.find((candidate) => candidate._id === id);
    if (!item) return;
    const sourceRow = Number(item.row);
    const sourcePosition = this._items(sourceRow).findIndex(
      (candidate) => candidate._id === id,
    );
    const target = this._items(row).filter((candidate) => candidate._id !== id);
    if (target.length >= 4) return;
    const rows = {
      1: this._items(1).filter((candidate) => candidate._id !== id),
      2: this._items(2).filter((candidate) => candidate._id !== id),
    };
    item.row = row;
    if (sourceRow === row && sourcePosition < position) position -= 1;
    rows[row].splice(Math.min(position, rows[row].length), 0, item);
    this._layout.items = [...rows[1], ...rows[2]];
    this._selectedId = id;
    this._notice = "";
    this._render();
  }

  _payload() {
    const items = this._layout.items.map(({ _id, ...item }) => ({
      ...item,
      decimals: Number(item.decimals),
      row: Number(item.row),
    }));
    return {
      title: this._layout.title,
      interval: Number(this._layout.interval),
      row_one_count: items.filter((item) => item.row === 1).length,
      row_two_count: items.filter((item) => item.row === 2).length,
      items,
    };
  }

  async _save() {
    if (this._saving) return;
    this._saving = true;
    this._notice = "Saving…";
    this._renderNotice();
    try {
      await this._hass.callWS({
        type: "ha_ink_display/save",
        entry_id: this._entryId,
        layout: this._payload(),
      });
      const entry = this._entries.find((item) => item.entry_id === this._entryId);
      if (entry) entry.layout = this._payload();
      this._notice = "Saved. The display will update on its next poll.";
    } catch (error) {
      this._notice = this._message(error);
    } finally {
      this._saving = false;
      this._renderNotice();
    }
  }

  _message(error) {
    const code = error?.code || "";
    const messages = {
      invalid_item_count: "Add at least one item to each row.",
      empty_row: "Each row must contain at least one item.",
      invalid_entity: "Select a Home Assistant entity for every item.",
      invalid_alert_threshold: "Enter a valid alert threshold.",
      invalid_length: "A label, unit, or heading is too long.",
      invalid_characters: "Use letters, numbers, spaces, º, °, and . : / % - only.",
    };
    return messages[error?.message] || messages[code] || error?.message || "Unable to save.";
  }

  _renderNotice() {
    const element = this.shadowRoot.querySelector("[data-notice]");
    if (element) element.textContent = this._notice;
  }

  _renderEmpty() {
    this.shadowRoot.innerHTML = `${this._styles()}<main class="empty">No paired Ink Display was found.</main>`;
  }

  _renderError(message) {
    this.shadowRoot.innerHTML = `${this._styles()}<main class="empty">${escapeHtml(message)}</main>`;
  }

  _card(item, position) {
    const entity = this._hass.states[item.entity];
    const state = entity?.state ?? "—";
    const unit = item.unit || entity?.attributes?.unit_of_measurement || "";
    const active = item._id === this._selectedId ? " active" : "";
    return `
      <button class="slot${active}" draggable="true" data-id="${item._id}" data-position="${position}">
        <span class="grip" aria-hidden="true">⠿</span>
        <span class="slot-label">${escapeHtml(item.label)}</span>
        <span class="slot-value">${escapeHtml(state)} <span>${escapeHtml(unit)}</span></span>
      </button>`;
  }

  _row(row) {
    const items = this._items(row);
    const cards = items.map((item, index) => this._card(item, index)).join("");
    const add = items.length < 4
      ? `<button class="add" data-add="${row}">+ Add item</button>`
      : "";
    const empty = items.length ? "" : `<div class="drop-empty">Drop an item here</div>`;
    return `
      <section class="display-row" data-row="${row}">
        <div class="row-name"><span>Row ${row}</span>${add}</div>
        <div class="slots" style="--columns:${Math.max(items.length, 1)}">${cards}${empty}</div>
      </section>`;
  }

  _editor() {
    const item = this._selected();
    if (!item) {
      return `<aside class="editor muted">Select an item or add one to a row.</aside>`;
    }
    const thresholdDisabled = item.alert_mode === "off" ? "disabled" : "";
    return `
      <aside class="editor">
        <div class="editor-bar">
          <strong>Item settings</strong>
          <button class="delete" data-delete>Remove</button>
        </div>
        <label>Home Assistant entity</label>
        <div data-entity-picker></div>
        <label for="label">Label <span>optional</span></label>
        <input id="label" data-field="label" maxlength="12" value="${escapeHtml(item.label)}">
        <label for="unit">Unit <span>optional, accepts º and °</span></label>
        <input id="unit" data-field="unit" maxlength="5" value="${escapeHtml(item.unit)}" placeholder="Use Home Assistant unit">
        <label for="decimals">Decimal places</label>
        <select id="decimals" data-field="decimals">
          ${[0, 1, 2].map((value) => `<option value="${value}" ${Number(item.decimals) === value ? "selected" : ""}>${value}</option>`).join("")}
        </select>
        <label for="alert_mode">Red alert</label>
        <select id="alert_mode" data-field="alert_mode">
          <option value="off" ${item.alert_mode === "off" ? "selected" : ""}>Never</option>
          <option value="above" ${item.alert_mode === "above" ? "selected" : ""}>When value is above</option>
          <option value="below" ${item.alert_mode === "below" ? "selected" : ""}>When value is below</option>
        </select>
        <label for="alert_threshold">Alert threshold</label>
        <input id="alert_threshold" data-field="alert_threshold" type="number" step="any" value="${escapeHtml(item.alert_threshold)}" ${thresholdDisabled}>
      </aside>`;
  }

  _render() {
    const entryOptions = this._entries.map((entry) => `
      <option value="${entry.entry_id}" ${entry.entry_id === this._entryId ? "selected" : ""}>${escapeHtml(entry.title)}</option>`).join("");
    this.shadowRoot.innerHTML = `
      ${this._styles()}
      <div class="toolbar">
        <select data-entry aria-label="Display">${entryOptions}</select>
        <span data-notice>${escapeHtml(this._notice)}</span>
        <button class="save" data-save ${this._saving ? "disabled" : ""}>Save changes</button>
      </div>
      <main>
        <div class="settings">
          <label for="title">Header</label>
          <input id="title" data-layout="title" maxlength="24" value="${escapeHtml(this._layout.title)}">
          <label for="interval">Minimum update interval</label>
          <select id="interval" data-layout="interval">
            ${[60, 300, 600, 900, 1800, 3600].map((value) => `<option value="${value}" ${Number(this._layout.interval) === value ? "selected" : ""}>${value < 60 ? value : `${value / 60} min`}</option>`).join("")}
          </select>
          <span class="fixed">Last update is always shown as ACT HH:MM.</span>
        </div>
        <div class="workspace">
          <div class="display">
            <div class="display-head"><strong>${escapeHtml(this._layout.title || "HOUSE")}</strong><span>ACT 12:30</span></div>
            ${this._row(1)}
            ${this._row(2)}
          </div>
          ${this._editor()}
        </div>
      </main>`;
    this._bind();
  }

  _bind() {
    this.shadowRoot.querySelector("[data-entry]")?.addEventListener("change", (event) => {
      this._selectEntry(event.target.value);
    });
    this.shadowRoot.querySelector("[data-save]")?.addEventListener("click", () => this._save());
    this.shadowRoot.querySelector("[data-delete]")?.addEventListener("click", () => this._remove());
    this.shadowRoot.querySelectorAll("[data-add]").forEach((button) => {
      button.addEventListener("click", () => this._add(Number(button.dataset.add)));
    });
    this.shadowRoot.querySelectorAll("[data-layout]").forEach((field) => {
      field.addEventListener("input", () => {
        this._layout[field.dataset.layout] = field.value;
        if (field.dataset.layout === "title") {
          this.shadowRoot.querySelector(".display-head strong").textContent = field.value || "HOUSE";
        }
      });
    });
    this.shadowRoot.querySelectorAll("[data-field]").forEach((field) => {
      field.addEventListener("input", () => {
        const item = this._selected();
        if (!item) return;
        item[field.dataset.field] = field.value;
        if (field.dataset.field === "alert_mode") this._render();
        if (field.dataset.field === "label") {
          this.shadowRoot.querySelector(`[data-id="${item._id}"] .slot-label`).textContent = field.value;
        }
      });
    });
    const pickerHost = this.shadowRoot.querySelector("[data-entity-picker]");
    const item = this._selected();
    if (pickerHost && item) {
      const picker = document.createElement("ha-entity-picker");
      picker.hass = this._hass;
      picker.value = item.entity;
      picker.includeDomains = ["sensor", "binary_sensor", "input_number", "number"];
      picker.addEventListener("value-changed", (event) => {
        item.entity = event.detail.value || "";
        this._render();
      });
      pickerHost.appendChild(picker);
    }
    this.shadowRoot.querySelectorAll(".slot").forEach((card) => {
      card.addEventListener("click", () => {
        this._selectedId = card.dataset.id;
        this._render();
      });
      card.addEventListener("dragstart", (event) => {
        event.dataTransfer.effectAllowed = "move";
        event.dataTransfer.setData("text/plain", card.dataset.id);
      });
      card.addEventListener("dragover", (event) => event.preventDefault());
      card.addEventListener("drop", (event) => {
        event.preventDefault();
        const row = Number(card.closest("[data-row]").dataset.row);
        this._move(event.dataTransfer.getData("text/plain"), row, Number(card.dataset.position));
      });
    });
    this.shadowRoot.querySelectorAll(".display-row").forEach((rowElement) => {
      rowElement.addEventListener("dragover", (event) => event.preventDefault());
      rowElement.addEventListener("drop", (event) => {
        if (event.target.closest(".slot")) return;
        event.preventDefault();
        const row = Number(rowElement.dataset.row);
        this._move(event.dataTransfer.getData("text/plain"), row, this._items(row).length);
      });
    });
  }

  _styles() {
    return `
      <style>
        :host { display: block; min-height: 100%; color: var(--primary-text-color); background: var(--primary-background-color); font-family: var(--paper-font-body1_-_font-family, sans-serif); }
        * { box-sizing: border-box; }
        button, input, select { font: inherit; }
        .toolbar { height: 64px; display: flex; align-items: center; gap: 16px; padding: 0 24px; border-bottom: 1px solid var(--divider-color); background: var(--app-header-background-color, var(--card-background-color)); }
        .toolbar select { min-width: 240px; }
        .toolbar span { flex: 1; color: var(--secondary-text-color); font-size: 14px; }
        main { max-width: 1180px; margin: 0 auto; padding: 24px; }
        .settings { display: grid; grid-template-columns: auto minmax(160px, 240px) auto minmax(130px, 180px) 1fr; gap: 10px 12px; align-items: center; margin-bottom: 24px; }
        .fixed { color: var(--secondary-text-color); font-size: 13px; }
        input, select { min-height: 40px; padding: 8px 10px; color: var(--primary-text-color); background: var(--card-background-color); border: 1px solid var(--divider-color); border-radius: 8px; }
        input:focus, select:focus { outline: 2px solid var(--primary-color); outline-offset: 1px; }
        input:disabled { opacity: .5; }
        .workspace { display: grid; grid-template-columns: minmax(0, 1fr) 320px; gap: 24px; align-items: start; }
        .display { overflow: hidden; border: 2px solid var(--primary-text-color); background: #f2f0e8; color: #171717; }
        .display-head { height: 52px; padding: 0 16px; display: flex; align-items: center; justify-content: space-between; color: #f2f0e8; background: #171717; }
        .display-row { min-height: 150px; padding: 12px; border-top: 1px solid #77736a; }
        .row-name { min-height: 28px; margin-bottom: 4px; display: flex; align-items: center; justify-content: space-between; color: #5a574f; font-size: 12px; }
        .slots { display: grid; grid-template-columns: repeat(var(--columns), minmax(0, 1fr)); min-height: 104px; }
        .slot { min-width: 0; border: 0; border-right: 1px solid #aaa69c; border-radius: 0; background: transparent; color: #171717; cursor: pointer; }
        .slot { position: relative; display: flex; flex-direction: column; align-items: flex-start; justify-content: center; gap: 8px; padding: 14px 10px; text-align: left; }
        .slot:last-child { border-right: 0; }
        .slot.active { outline: 3px solid var(--primary-color); outline-offset: -3px; }
        .slot-label { min-height: 16px; font-size: 13px; font-weight: 600; }
        .slot-value { overflow: hidden; max-width: 100%; font-size: 26px; font-weight: 700; white-space: nowrap; }
        .slot-value span { font-size: 12px; font-weight: 500; }
        .grip { position: absolute; top: 4px; right: 6px; color: #77736a; cursor: grab; }
        .add { padding: 4px 8px; border: 1px solid #aaa69c; border-radius: 6px; color: #5a574f; background: transparent; cursor: pointer; }
        .drop-empty { display: grid; place-items: center; border: 1px dashed #aaa69c; color: #77736a; font-size: 13px; }
        .editor { padding: 18px; display: grid; gap: 8px; border: 1px solid var(--divider-color); border-radius: 10px; background: var(--card-background-color); }
        .editor label { margin-top: 6px; font-size: 13px; font-weight: 600; }
        .editor label span { color: var(--secondary-text-color); font-weight: 400; }
        .editor ha-entity-picker { display: block; width: 100%; }
        .editor-bar { display: flex; align-items: center; justify-content: space-between; margin-bottom: 4px; }
        .muted { color: var(--secondary-text-color); }
        .save, .delete { min-height: 38px; padding: 8px 14px; border-radius: 8px; cursor: pointer; }
        .save { border: 0; color: var(--text-primary-color, #fff); background: var(--primary-color); }
        .save:disabled { opacity: .6; }
        .delete { border: 1px solid var(--error-color); color: var(--error-color); background: transparent; }
        .empty { padding: 32px; color: var(--secondary-text-color); }
        @media (max-width: 850px) {
          .toolbar { padding: 0 16px; }
          .toolbar span { display: none; }
          main { padding: 16px; }
          .settings { grid-template-columns: 1fr; align-items: stretch; }
          .workspace { grid-template-columns: 1fr; }
          .slot { min-height: 94px; }
        }
      </style>`;
  }
}

customElements.define("ha-ink-display-panel", HaInkDisplayPanel);
