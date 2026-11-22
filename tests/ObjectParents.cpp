/**
 * @file
 * @brief Unit tests for clip and tracked-object parenting
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

// Copyright (c) 2008-2026 OpenShot Studios, LLC
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include "openshot_catch.h"
#include "Clip.h"
#include "DummyReader.h"
#include "Timeline.h"
#include <QTemporaryDir>
#include <set>
#include <fstream>
#include <vector>

#ifdef USE_OPENCV
#include "objdetectdata.pb.h"
#include "TrackedObjectBBox.h"
#include "effects/ObjectDetection.h"
#include "effects/Tracker.h"
#endif

using namespace openshot;

TEST_CASE("A clip with timeline context can outlive an unregistered timeline", "[parenting]") {
	DummyReader reader;
	auto clip = std::make_unique<Clip>(&reader);
	{
		Timeline context(32, 32, Fraction(60, 1), 48000, 2, LAYOUT_STEREO);
		clip->ParentTimeline(&context);
		CHECK(context.Clips().empty());
		CHECK(clip->ParentTimeline() == &context);
	}
	// ParentTimeline supplies rendering/FPS context without registering a clip.
	// Destruction must not call back into the already-destroyed context.
	CHECK_NOTHROW(clip.reset());
}

TEST_CASE("Borrowed preview clips can be destroyed before their timeline", "[parenting]") {
	DummyReader reader;
	auto timeline = std::make_unique<Timeline>(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline->AutoMapClips(GENERATE(false, true));
	auto preview = std::make_unique<Clip>(&reader);
	preview->End(2.0);
	timeline->AddClip(preview.get());
	const auto id = preview->Id();
	// Match SelectRegion: close playback, release the Python-owned clip, then
	// release the temporary timeline later when the dialog is collected.
	timeline->Open();
	timeline->GetFrame(1);
	timeline->Close();
	preview.reset();
	CHECK(timeline->GetClip(id) == nullptr);
	CHECK(timeline->Clips().empty());
	CHECK_NOTHROW(timeline->Clear());
	timeline.reset();
}

TEST_CASE("Deleted clip parents are ignored and restored by ID", "[parenting]") {
	DummyReader reader(Fraction(24, 1), 32, 32, 48000, 2, 3.0);
	Clip source(&reader), child(&reader), restored(&reader);
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	source.Id("source");
	source.location_x = Keyframe(0.25);
	source.End(2.0);
	Json::Value project;
	project["clips"].append(source.JsonValue());
	timeline.SetJsonValue(project);
	auto owned_parent = timeline.GetClip("source");
	REQUIRE(owned_parent);
	timeline.AddClip(&child);
	child.SetAttachedClip(owned_parent);
	CHECK(child.GetAttachedClip() == owned_parent);
	CHECK(stringToJson(child.PropertiesJSON(1))["location_x"]["value"].asDouble() == Approx(0.25));
	// Populate the open-clip registry, then delete a timeline-owned parent.
	timeline.Open();
	auto before = timeline.GetFrame(1);
	timeline.RemoveClip(owned_parent);
	CHECK(timeline.GetClip("source") == nullptr);
	CHECK(child.GetParentClip() == nullptr);
	CHECK(child.GetAttachedClip() == nullptr);
	CHECK(child.GetAttachedId() == "source");
	CHECK(stringToJson(child.PropertiesJSON(1))["location_x"]["value"].asDouble() == Approx(0.0));
	CHECK(timeline.GetFrame(1) != before);

	restored.Id("source");
	restored.location_x = Keyframe(-0.25);
	timeline.AddClip(&restored);
	CHECK(child.GetParentClip() == &restored);
	CHECK(stringToJson(child.PropertiesJSON(1))["location_x"]["value"].asDouble() == Approx(-0.25));
	child.SetAttachedId("missing");
	CHECK(child.GetAttachedClip() == nullptr);
	child.AttachToObject(child.Id());
	CHECK(child.GetParentClip() == nullptr);
}

TEST_CASE("Parent resolution follows timeline changes and invalid IDs", "[parenting]") {
	DummyReader reader;
	Clip parent(&reader), replacement(&reader), child(&reader);
	Timeline first(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	Timeline second(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	first.AutoMapClips(false);
	second.AutoMapClips(false);
	parent.Id("parent");
	replacement.Id("parent");
	first.AddClip(&parent);
	first.AddClip(&child);
	child.AttachToObject("parent");
	REQUIRE(child.GetParentClip() == &parent);
	Json::Value update;
	update["parentObjectId"] = "missing";
	child.SetJsonValue(update);
	CHECK(child.GetAttachedClip() == nullptr);
	child.AttachToObject("parent");
	first.RemoveClip(&child);
	CHECK(child.GetParentClip() == nullptr);
	second.AddClip(&child);
	CHECK(child.GetParentClip() == nullptr);
	second.AddClip(&replacement);
	CHECK(child.GetParentClip() == &replacement);
	second.Clear();
	CHECK(child.GetParentClip() == nullptr);
	CHECK(child.ParentTimeline() == nullptr);
}

#ifdef USE_OPENCV
TEST_CASE("Deleting a tracking effect through JSON removes its clip attachment", "[parenting][opencv]") {
	DummyReader reader;
	Clip source(&reader), child(&reader);
	Tracker tracker;
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	timeline.AddClip(&source);
	timeline.AddClip(&child);
	source.AddEffect(&tracker);
	child.AttachToObject(tracker.trackedData->Id());
	REQUIRE(child.GetAttachedObject());
	Json::Value change;
	change["type"] = "delete";
	change["key"].append("clips");
	change["key"][1]["id"] = source.Id();
	change["key"].append("effects");
	change["key"][3]["id"] = tracker.Id();
	Json::Value changes(Json::arrayValue);
	changes.append(change);
	timeline.ApplyJsonDiff(changes.toStyledString());
	CHECK(source.Effects().empty());
	CHECK(child.GetAttachedObject() == nullptr);
	CHECK(timeline.GetTrackedObjectsIds().empty());
}

TEST_CASE("Deleting a timeline-owned tracked source releases parent data", "[parenting][opencv]") {
	DummyReader reader;
	Clip source(&reader), child(&reader);
	Tracker tracker;
	source.AddEffect(&tracker);
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	Json::Value project;
	project["clips"].append(source.JsonValue());
	timeline.SetJsonValue(project);
	timeline.AddClip(&child);
	const auto id = tracker.trackedData->Id();
	child.AttachToObject(id);
	auto loaded_object = child.GetAttachedObject();
	REQUIRE(loaded_object);
	timeline.RemoveClip(timeline.GetClip(source.Id()));
	CHECK(child.GetAttachedObject() == nullptr);
	CHECK(loaded_object->ParentClip() == nullptr);
	CHECK(timeline.GetTrackedObject(id) == nullptr);
	CHECK(stringToJson(timeline.GetTrackedObjectValues(id, 1))["x1"].asDouble() == 0.0);
	// A replacement project must not retain any old tracked objects either.
	timeline.SetJsonValue(project);
	REQUIRE(timeline.GetTrackedObject(id));
	project["clips"] = Json::Value(Json::arrayValue);
	timeline.SetJsonValue(project);
	CHECK(timeline.GetTrackedObject(id) == nullptr);
}

TEST_CASE("Removing tracking effects releases objects and permits undo", "[parenting][opencv]") {
	DummyReader reader;
	Clip source(&reader), child(&reader), later_child(&reader);
	Tracker tracker;
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	// Effects can be constructed before a clip joins the timeline.
	source.AddEffect(&tracker);
	timeline.AddClip(&source);
	timeline.AddClip(&child);
	const auto id = tracker.trackedData->Id();
	child.AttachToObject(id);
	REQUIRE(child.GetAttachedObject() == tracker.trackedData);
	CHECK(tracker.trackedData->ParentClip() == &source);
	source.RemoveEffect(&tracker);
	CHECK(timeline.GetTrackedObjectsIds().empty());
	CHECK(timeline.GetTrackedObject(id) == nullptr);
	CHECK(child.GetParentTrackedObject() == nullptr);
	CHECK(child.GetAttachedObject() == nullptr);
	CHECK(tracker.trackedData->ParentClip() == nullptr);
	CHECK(tracker.ParentClip() == nullptr);
	CHECK(tracker.ParentTimeline() == nullptr);
	timeline.AddClip(&later_child);
	later_child.AttachToObject(id);
	CHECK(later_child.GetParentTrackedObject() == nullptr);
	source.AddEffect(&tracker);
	CHECK(child.GetParentTrackedObject() == tracker.trackedData);
	CHECK(later_child.GetParentTrackedObject() == tracker.trackedData);
	timeline.RemoveClip(&source);
	CHECK(child.GetAttachedObject() == nullptr);
	CHECK(timeline.GetTrackedObject(id) == nullptr);
	CHECK(tracker.trackedData->ParentClip() == nullptr);
	timeline.AddClip(&source);
	CHECK(child.GetParentTrackedObject() == tracker.trackedData);
}

TEST_CASE("Replacing tracked data resolves the current object without retaining old data", "[parenting][opencv]") {
	DummyReader reader;
	Clip source(&reader), child(&reader);
	Tracker tracker;
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	timeline.AddClip(&source);
	timeline.AddClip(&child);
	source.AddEffect(&tracker);
	const auto id = tracker.trackedData->Id();
	child.SetAttachedObject(tracker.trackedData);
	std::weak_ptr<TrackedObjectBase> old_object = tracker.trackedData;
	tracker.trackedData = std::make_shared<TrackedObjectBBox>();
	tracker.trackedData->Id(id);
	tracker.trackedObjects[0] = tracker.trackedData;
	CHECK(child.GetAttachedObject() == tracker.trackedData);
	CHECK(old_object.expired());
	CHECK(tracker.trackedData->ParentClip() == &source);
	Json::Value update;
	update["effects"] = Json::Value(Json::arrayValue);
	source.SetJsonValue(update);
	CHECK(tracker.ParentClip() == nullptr);
	CHECK(tracker.ParentTimeline() == nullptr);
	CHECK(child.GetParentTrackedObject() == nullptr);
	CHECK(timeline.GetTrackedObjectsIds().empty());
	CHECK(tracker.trackedData->ParentClip() == nullptr);
}

TEST_CASE("Trackers and object detectors have independent parents through save and reload", "[parenting][opencv]") {
	QTemporaryDir temp;
	REQUIRE(temp.isValid());
	const auto path = temp.filePath("objects.data").toStdString();
	Json::Value config;
	config["protobuf_data_path"] = path;
	pb_objdetect::ObjDetect data;
	data.add_classnames("object");
	auto frame = data.add_frame();
	frame->set_id(1);
	auto box = frame->add_bounding_box();
	box->set_x(0.1f);
	box->set_y(0.1f);
	box->set_w(0.2f);
	box->set_h(0.2f);
	box->set_classid(0);
	box->set_objectid(0);
	box->set_confidence(0.9f);
	{
		std::ofstream output(path, std::ios::binary);
		REQUIRE(data.SerializeToOstream(&output));
	}
	DummyReader reader;
	Clip first(&reader), second(&reader);
	Tracker tracker1, tracker2;
	ObjectDetection detector1, detector2;
	detector1.SetJsonValue(config);
	detector2.SetJsonValue(config);
	REQUIRE(detector1.trackedObjects.size() == 1);
	REQUIRE(detector2.trackedObjects.size() == 1);
	first.AddEffect(&tracker1);
	first.AddEffect(&detector1);
	const bool split_clips = GENERATE(false, true);
	auto& other = split_clips ? second : first;
	other.AddEffect(&tracker2);
	other.AddEffect(&detector2);
	Timeline timeline(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	timeline.AutoMapClips(false);
	timeline.AddClip(&first);
	timeline.AddClip(&second);
	std::vector<std::unique_ptr<Clip>> children;
	std::set<std::string> ids;
	for (auto effect : std::vector<EffectBase*>{&tracker1, &detector1, &tracker2, &detector2}) {
		auto object = effect->trackedObjects.at(0);
		REQUIRE(ids.insert(object->Id()).second);
		CHECK(timeline.GetTrackedObject(object->Id()) == object);
		CHECK(object->ParentClip() == effect->ParentClip());
		children.emplace_back(new Clip(&reader));
		timeline.AddClip(children.back().get());
		children.back()->AttachToObject(object->Id());
		CHECK(children.back()->GetParentTrackedObject() == object);
	}
	CHECK(timeline.GetTrackedObjectsIds().size() == 4);
	Timeline restored(32, 32, Fraction(24, 1), 48000, 2, LAYOUT_STEREO);
	restored.AutoMapClips(false);
	restored.SetJsonValue(timeline.JsonValue());
	CHECK(restored.GetTrackedObjectsIds().size() == 4);
	for (const auto& child : children) {
		auto loaded_child = restored.GetClip(child->Id());
		REQUIRE(loaded_child);
		auto loaded_object = loaded_child->GetParentTrackedObject();
		REQUIRE(loaded_object);
		CHECK(loaded_object->Id() == child->GetAttachedId());
		CHECK(loaded_object != child->GetParentTrackedObject());
	}
	first.RemoveEffect(&detector1);
	CHECK(children[1]->GetParentTrackedObject() == nullptr);
	CHECK(children[0]->GetParentTrackedObject() == tracker1.trackedData);
	CHECK(children[2]->GetParentTrackedObject() == tracker2.trackedData);
	CHECK(children[3]->GetParentTrackedObject() == detector2.trackedObjects.at(0));
	timeline.Clear();
	CHECK(timeline.GetTrackedObjectsIds().empty());
	for (const auto& child : children)
		CHECK(child->GetAttachedObject() == nullptr);
}
#endif
