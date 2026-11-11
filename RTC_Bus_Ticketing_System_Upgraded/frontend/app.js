let currentBuses = [];
let selectedBus = null;
let selectedSeat = null;
let currentTicket = null;
let lastSearch = { from: "Vijayawada", to: "Hyderabad" };
const $ = id => document.getElementById(id);
const money = value => "₹" + Number(value).toFixed(2);
const escapeHTML = value => String(value ?? "").replace(/[&<>"']/g, ch => ({"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[ch]));

async function api(url, options = {}) {
  const response = await fetch(url, { headers: { "Content-Type": "application/json" }, ...options });
  const data = await response.json();
  if (!response.ok || data.error) throw new Error(data.error || "Something went wrong. Please try again.");
  return data;
}
function showPage(name) {
  document.querySelectorAll(".page").forEach(page => page.classList.toggle("active", page.id === "page-" + name));
  document.querySelectorAll(".nav-link").forEach(link => link.classList.toggle("active", link.dataset.page === name));
  window.scrollTo({ top: 0, behavior: "smooth" });
}
document.addEventListener("click", event => {
  const link = event.target.closest("[data-page]");
  if (link) { event.preventDefault(); showPage(link.dataset.page); }
});
function setDefaultDate() {
  const today = new Date();
  const local = new Date(today.getTime() - today.getTimezoneOffset() * 60000).toISOString().slice(0, 10);
  $("journeyDate").min = local;
  $("journeyDate").value = local;
}
function detailRows(rows) {
  return rows.map(([label, value]) => `<div class="detail-row"><span>${escapeHTML(label)}</span><b>${escapeHTML(value)}</b></div>`).join("");
}
function busCard(bus) {
  return `<article class="bus-card">
    <div><span class="bus-category">${escapeHTML(bus.category)}</span><h3>${escapeHTML(bus.busNumber)}</h3><p>RTC sample service</p></div>
    <div class="bus-route">${escapeHTML(bus.source)} <span>→</span> ${escapeHTML(bus.destination)}<small>${bus.distance} km journey</small></div>
    <div class="bus-stat"><span>AVAILABLE SEATS</span><b>${bus.availableSeats}/24</b></div>
    <div class="bus-stat"><span>FARE PER SEAT</span><b class="fare">${money(bus.fare)}</b></div>
    <button class="primary-btn select-bus" data-bus="${escapeHTML(bus.busNumber)}">Select bus →</button>
  </article>`;
}
async function searchBuses(from, to) {
  const message = $("searchError");
  message.textContent = "";
  if (!from || !to || from.toLowerCase() === to.toLowerCase()) {
    message.textContent = "Please choose two different locations.";
    showPage("home");
    return;
  }
  lastSearch = { from, to };
  try {
    currentBuses = await api(`/api/buses?from=${encodeURIComponent(from)}&to=${encodeURIComponent(to)}`);
    $("resultsRoute").textContent = `${from} → ${to} · ${$("journeyDate").value}`;
    $("resultCount").textContent = `${currentBuses.length} buses found`;
    $("categoryFilter").value = "all";
    renderBuses();
    showPage("results");
  } catch (error) {
    message.textContent = error.message;
    showPage("home");
  }
}
function renderBuses() {
  const category = $("categoryFilter").value;
  const filtered = category === "all" ? currentBuses : currentBuses.filter(bus => bus.category === category);
  $("busResults").innerHTML = filtered.map(busCard).join("");
  $("noResults").classList.toggle("hidden", filtered.length !== 0);
  $("busResults").classList.toggle("hidden", filtered.length === 0);
}
async function chooseBus(busNumber) {
  selectedBus = currentBuses.find(bus => bus.busNumber === busNumber);
  selectedSeat = null;
  if (!selectedBus) return;
  $("seatBusTitle").textContent = `${selectedBus.category} · ${selectedBus.busNumber}`;
  $("seatBusDetails").innerHTML = detailRows([
    ["Route", `${selectedBus.source} → ${selectedBus.destination}`],
    ["Distance", `${selectedBus.distance} km`],
    ["Available", `${selectedBus.availableSeats} seats`]
  ]);
  $("seatFare").textContent = money(selectedBus.fare);
  $("continuePassenger").disabled = true;
  $("seatGrid").innerHTML = "<p class='muted'>Loading seats…</p>";
  showPage("seats");
  try {
    const data = await api(`/api/seats?bus=${encodeURIComponent(busNumber)}`);
    $("seatGrid").innerHTML = data.seats.map((booked, index) =>
      `<button class="seat-btn" data-seat="${index + 1}" ${booked ? "disabled" : ""} aria-label="Seat ${index + 1}${booked ? ", booked" : ", available"}">${String(index + 1).padStart(2, "0")}</button>`
    ).join("");
  } catch (error) {
    $("seatGrid").textContent = error.message;
  }
}
function selectSeat(number, button) {
  selectedSeat = number;
  document.querySelectorAll(".seat-btn").forEach(seat => seat.classList.remove("selected"));
  button.classList.add("selected");
  $("continuePassenger").disabled = false;
}
function renderSummary(target, fareTarget) {
  $(target).innerHTML = detailRows([
    ["Bus", `${selectedBus.category} (${selectedBus.busNumber})`],
    ["Journey", `${selectedBus.source} → ${selectedBus.destination}`],
    ["Date", $("journeyDate").value],
    ["Seat", selectedSeat]
  ]);
  $(fareTarget).textContent = money(selectedBus.fare);
}
async function submitBooking(event) {
  event.preventDefault();
  $("bookingError").textContent = "";
  const name = $("passengerName").value.trim();
  const phone = $("passengerPhone").value.trim();
  if (!name) { $("bookingError").textContent = "Please enter the passenger name."; return; }
  if (!/^\d{10}$/.test(phone)) { $("bookingError").textContent = "Enter a valid 10-digit mobile number."; return; }
  try {
    const ticket = await api("/api/book", {
      method: "POST",
      body: JSON.stringify({ busNumber: selectedBus.busNumber, seat: selectedSeat, name, phone, journeyDate: $("journeyDate").value })
    });
    currentTicket = ticket;
    renderTicket(ticket, "confirmedTicket");
    showPage("confirmation");
    // Refresh availability in the background so a later search shows the updated count.
    currentBuses = await api(`/api/buses?from=${encodeURIComponent(lastSearch.from)}&to=${encodeURIComponent(lastSearch.to)}`);
  } catch (error) {
    $("bookingError").textContent = error.message;
  }
}
function renderTicket(ticket, targetId) {
  $(targetId).innerHTML = `
    <span class="ticket-status">${escapeHTML(ticket.status)}</span>
    <h2>${escapeHTML(ticket.source)} <span>→</span> ${escapeHTML(ticket.destination)}</h2>
    <p class="muted">RTC Bus Reservation · ${escapeHTML(ticket.bookingID)}</p>
    <div class="ticket-grid">
      <div class="ticket-field"><small>PASSENGER</small><b>${escapeHTML(ticket.name)}</b></div>
      <div class="ticket-field"><small>PHONE</small><b>${escapeHTML(ticket.phone)}</b></div>
      <div class="ticket-field"><small>BUS / CATEGORY</small><b>${escapeHTML(ticket.busNumber)} · ${escapeHTML(ticket.category)}</b></div>
      <div class="ticket-field"><small>SEAT NUMBER</small><b>${escapeHTML(ticket.seat)}</b></div>
      <div class="ticket-field"><small>JOURNEY DATE</small><b>${escapeHTML(ticket.journeyDate || "Not recorded")}</b></div>
      <div class="ticket-field"><small>BOOKING ID</small><b>${escapeHTML(ticket.bookingID)}</b></div>
    </div>
    <div class="ticket-total"><span>Total fare</span><b>${money(ticket.fare)}</b></div>`;
}
async function viewManagedTicket() {
  const id = $("bookingId").value.trim();
  $("manageMessage").textContent = "";
  if (!id) { $("manageMessage").textContent = "Enter a booking ID."; return; }
  try {
    const ticket = await api(`/api/ticket?id=${encodeURIComponent(id)}`);
    renderTicket(ticket, "managedTicket");
    $("managedTicket").classList.remove("managed-placeholder");
  } catch (error) {
    $("manageMessage").textContent = error.message;
    $("managedTicket").innerHTML = `<div class="placeholder-icon">!</div><h3>Ticket not found</h3><p class="muted">${escapeHTML(error.message)}</p>`;
  }
}
async function cancelManagedTicket() {
  const id = $("bookingId").value.trim();
  $("manageMessage").textContent = "";
  if (!id) { $("manageMessage").textContent = "Enter a booking ID first."; return; }
  if (!confirm(`Cancel booking ${id}?`)) return;
  try {
    const ticket = await api("/api/cancel", { method: "POST", body: JSON.stringify({ bookingID: id }) });
    renderTicket(ticket, "managedTicket");
    $("managedTicket").classList.remove("managed-placeholder");
    $("manageMessage").textContent = "Ticket cancelled. The seat has been released.";
  } catch (error) {
    $("manageMessage").textContent = error.message;
  }
}
$("searchForm").addEventListener("submit", event => {
  event.preventDefault();
  searchBuses($("from").value, $("to").value);
});
$("swap").addEventListener("click", () => {
  const from = $("from").value;
  $("from").value = $("to").value;
  $("to").value = from;
});
document.querySelectorAll(".route-card").forEach(card => card.addEventListener("click", () => {
  $("from").value = card.dataset.from;
  $("to").value = card.dataset.to;
  searchBuses(card.dataset.from, card.dataset.to);
}));
$("categoryFilter").addEventListener("change", renderBuses);
$("busResults").addEventListener("click", event => {
  const button = event.target.closest("[data-bus]");
  if (button) chooseBus(button.dataset.bus);
});
$("seatGrid").addEventListener("click", event => {
  const button = event.target.closest("[data-seat]");
  if (button && !button.disabled) selectSeat(Number(button.dataset.seat), button);
});
$("continuePassenger").addEventListener("click", () => {
  if (!selectedBus || !selectedSeat) return;
  renderSummary("passengerSummary", "passengerFare");
  $("bookingError").textContent = "";
  showPage("passenger");
});
$("passengerForm").addEventListener("submit", submitBooking);
$("printTicket").addEventListener("click", () => window.print());
$("manageForm").addEventListener("submit", event => { event.preventDefault(); viewManagedTicket(); });
$("cancelTicket").addEventListener("click", cancelManagedTicket);
setDefaultDate();

