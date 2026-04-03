#include "../src/sequencer.hh"

#include "doctest.h"

using namespace Catalyst2;

TEST_CASE("IncRatchetRepeat - CW from neutral enters ratchet mode") {
	Sequencer::Step s;
	CHECK(s.ReadRatchetRepeatCount() == 0);
	CHECK(s.IsRepeat() == false);
	s.IncRatchetRepeat(1);
	CHECK(s.IsRepeat() == false);
	CHECK(s.ReadRatchetRepeatCount() == 1);
	s.IncRatchetRepeat(1);
	CHECK(s.ReadRatchetRepeatCount() == 2);
}

TEST_CASE("IncRatchetRepeat - CCW from neutral enters repeat mode") {
	Sequencer::Step s;
	s.IncRatchetRepeat(-1);
	CHECK(s.IsRepeat() == true);
	CHECK(s.ReadRatchetRepeatCount() == 1);
	s.IncRatchetRepeat(-1);
	CHECK(s.IsRepeat() == true);
	CHECK(s.ReadRatchetRepeatCount() == 2);
}

TEST_CASE("IncRatchetRepeat - CW from repeat passes through neutral") {
	Sequencer::Step s;
	s.IncRatchetRepeat(-1); // enter repeat x1
	CHECK(s.IsRepeat() == true);
	s.IncRatchetRepeat(1); // back to neutral
	CHECK(s.IsRepeat() == false);
	CHECK(s.ReadRatchetRepeatCount() == 0);
	s.IncRatchetRepeat(1); // now enter ratchet x1
	CHECK(s.IsRepeat() == false);
	CHECK(s.ReadRatchetRepeatCount() == 1);
}

TEST_CASE("IncRatchetRepeat - CCW from ratchet passes through neutral") {
	Sequencer::Step s;
	s.IncRatchetRepeat(1); // enter ratchet x1
	CHECK(s.IsRepeat() == false);
	s.IncRatchetRepeat(-1); // back to neutral
	CHECK(s.IsRepeat() == false);
	CHECK(s.ReadRatchetRepeatCount() == 0);
	s.IncRatchetRepeat(-1); // now enter repeat x1
	CHECK(s.IsRepeat() == true);
	CHECK(s.ReadRatchetRepeatCount() == 1);
}

TEST_CASE("IncRatchetRepeat - clamps at max in both modes") {
	Sequencer::Step s;
	for (auto i = 0; i < 20; i++) s.IncRatchetRepeat(1);
	CHECK(s.ReadRatchetRepeatCount() == 7);
	for (auto i = 0; i < 20; i++) s.IncRatchetRepeat(-1);
	CHECK(s.IsRepeat() == true);
	CHECK(s.ReadRatchetRepeatCount() == 7);
}

TEST_CASE("IncRatchetRepeat - ReadRetrig returns 0 in repeat mode") {
	Sequencer::Step s;
	s.IncRatchetRepeat(-1);
	s.IncRatchetRepeat(-1); // repeat x2
	CHECK(s.IsRepeat() == true);
	CHECK(s.ReadRatchetRepeatCount() == 2);
	CHECK(s.ReadRetrig() == 0); // repeat mode: retrig returns 0
}


TEST_CASE("Step::ToggleSubStepMask - basic toggling") {
	Sequencer::Step s;
	CHECK(s.ReadSubStepMask() == 0xFF); // default: all on

	// Toggle off a sub-step
	s.ToggleSubStepMask(3);
	CHECK((s.ReadSubStepMask() & (1u << 3)) == 0);
	CHECK((s.ReadSubStepMask() & 1u) != 0); // bit 0 always set

	// Toggle it back on
	s.ToggleSubStepMask(3);
	CHECK((s.ReadSubStepMask() & (1u << 3)) != 0);

	// Sub-step 0 can be toggled off
	s.ToggleSubStepMask(0);
	CHECK((s.ReadSubStepMask() & 1u) == 0);

	// Toggle all sub-steps off
	for (uint8_t i = 1; i < 8; i++) {
		s.ToggleSubStepMask(i);
	}
	CHECK(s.ReadSubStepMask() == 0x00); // all sub-steps silenced

	CHECK(s.Validate() == true);
}

TEST_CASE("Sequencer::Data::validate - version tag is a required condition") {
	Sequencer::Data data;
	// Default-constructed tag is 0 — always fails regardless of other fields.
	CHECK(data.validate() == false);

	// Correct tag is necessary but slot data must also be valid.
	// Confirm: wrong tag always returns false even if we flip it back and forth.
	data.SettingsVersionTag = Sequencer::Data::current_tag;
	const auto valid_with_correct_tag = data.validate();
	data.SettingsVersionTag = 0;
	CHECK(data.validate() == false);
	data.SettingsVersionTag = 99;
	CHECK(data.validate() == false);
	// Restoring correct tag returns to whatever the slot data validates as.
	data.SettingsVersionTag = Sequencer::Data::current_tag;
	CHECK(data.validate() == valid_with_correct_tag);
}

TEST_CASE("Rotate steps") {

	Shared::Data shared_data;
	Shared::Interface shared{shared_data};
	Sequencer::Data data;
	Sequencer::Interface seq{data, shared};
	Channel::Cv::Range range{};

	// Fill a page with some values
	seq.slot.channel[0][0].IncCv(10, false, range);
	seq.slot.channel[0][1].IncCv(20, false, range);
	seq.slot.channel[0][2].IncCv(30, false, range);
	seq.slot.channel[0][3].IncCv(40, false, range);
	seq.slot.channel[0][4].IncCv(50, false, range);
	seq.slot.channel[0][5].IncCv(60, false, range);
	seq.slot.channel[0][6].IncCv(70, false, range);
	seq.slot.channel[0][7].IncCv(80, false, range);

	auto f = [&]() {
		std::array<uint16_t, 8> out;
		for (auto i = 0u; i < out.size(); i++) {
			out[i] = seq.GetStep(i).ReadCv();
		}
		return out;
	};

	SUBCASE("Forwards") {
		auto original_vals = f();
		seq.RotateStepsRight(0, 7);
		auto rotated_vals = f();

		CHECK(rotated_vals[0] == original_vals[7]);
		CHECK(rotated_vals[1] == original_vals[0]);
		CHECK(rotated_vals[2] == original_vals[1]);
		CHECK(rotated_vals[3] == original_vals[2]);
		CHECK(rotated_vals[4] == original_vals[3]);
		CHECK(rotated_vals[5] == original_vals[4]);
		CHECK(rotated_vals[6] == original_vals[5]);
		CHECK(rotated_vals[7] == original_vals[6]);
	}

	SUBCASE("Backwards") {
		auto original_vals = f();
		seq.RotateStepsLeft(0, 7);
		auto rotated_vals = f();

		CHECK(original_vals[0] == rotated_vals[7]);
		CHECK(original_vals[1] == rotated_vals[0]);
		CHECK(original_vals[2] == rotated_vals[1]);
		CHECK(original_vals[3] == rotated_vals[2]);
		CHECK(original_vals[4] == rotated_vals[3]);
		CHECK(original_vals[5] == rotated_vals[4]);
		CHECK(original_vals[6] == rotated_vals[5]);
		CHECK(original_vals[7] == rotated_vals[6]);
	}
}

TEST_CASE("Random Gate Prob.") {
	Catalyst2::Sequencer::Step gate;
	CHECK(gate.ReadGate() == 0.f);
	gate.IncGate(1);
	CHECK(gate.ReadGate() > 0.f);

	// Set to 0
	gate.IncGate(-1);
	CHECK(gate.ReadGate() == 0.f);

	// Random shift folds back at 0

	CHECK(gate.ReadGate(1.f * 0.011f) == doctest::Approx(0.011));
	CHECK(gate.ReadGate(-1.f * 0.011f) == doctest::Approx(0.011));

	gate.IncGate(1);
	auto base = gate.ReadGate(); // 0.004

	CHECK(gate.ReadGate(1.f * 0.011f) == doctest::Approx(0.011 + (double)base));
	CHECK(gate.ReadGate(-1.f * 0.011f) == doctest::Approx(0.011 - (double)base));

	CHECK(gate.ReadGate(2.f * base) == (3 * base));
	CHECK(gate.ReadGate(-2.f * base) == base);

	// Set to Max
	gate.IncGate(100);
	CHECK(gate.ReadGate() == 1.f);

	// Folds back at max
	CHECK(gate.ReadGate(1.f * 0.011f) == doctest::Approx(0.989));
	CHECK(gate.ReadGate(-1.f * 0.011f) == doctest::Approx(0.989));
}
