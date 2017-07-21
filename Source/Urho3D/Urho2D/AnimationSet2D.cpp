//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include "../Precompiled.h"

#include "../Urho2D/AnimationSet2D.h"
#include "../Math/AreaAllocator.h"
#include "../Container/ArrayPtr.h"
#include "../Core/Context.h"
#include "../IO/FileSystem.h"
#include "../Graphics/Graphics.h"
#include "../Resource/Image.h"
#include "../IO/Log.h"
#include "../Resource/ResourceCache.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/SpriterData2D.h"
#include "../Urho2D/SpriteSheet2D.h"
#include "../Graphics/Texture2D.h"
#include "../Resource/XMLFile.h"

#include "../DebugNew.h"

#ifdef URHO3D_SPINE
#include <spine/spine.h>
#include <spine/extension.h>
#endif

#ifdef URHO3D_SPINE
// Current animation set
static Urho3D::AnimationSet2D* currentAnimationSet = 0;

void _spAtlasPage_createTexture(spAtlasPage* self, const char* path)
{
    using namespace Urho3D;
    if (!currentAnimationSet)
        return;

    ResourceCache* cache = currentAnimationSet->GetSubsystem<ResourceCache>();
    Sprite2D* sprite = cache->GetResource<Sprite2D>(path);
    // Add reference
    if (sprite)
        sprite->AddRef();

    self->width = sprite->GetTexture()->GetWidth();
    self->height = sprite->GetTexture()->GetHeight();

    self->rendererObject = sprite;
}

void _spAtlasPage_disposeTexture(spAtlasPage* self)
{
    using namespace Urho3D;
    Sprite2D* sprite = static_cast<Sprite2D*>(self->rendererObject);
    if (sprite)
        sprite->ReleaseRef();

    self->rendererObject = 0;
}

char* _spUtil_readFile(const char* path, int* length)
{
    using namespace Urho3D;

    if (!currentAnimationSet)
        return 0;

    ResourceCache* cache = currentAnimationSet->GetSubsystem<ResourceCache>();
    SharedPtr<File> file = cache->GetFile(path);
    if (!file)
        return 0;

    unsigned size = file->GetSize();

    char* data = MALLOC(char, size + 1);
    file->Read(data, size);
    data[size] = '\0';

    file.Reset();
    *length = size;

    return data;
}
#endif

namespace Urho3D
{

AnimationSet2D::AnimationSet2D(Context* context) :
    Resource(context),
#ifdef URHO3D_SPINE
    skeletonData_(0),
    atlas_(0),
#endif
    hasSpriteSheet_(false)
{
}

AnimationSet2D::~AnimationSet2D()
{
    Dispose();
}

void AnimationSet2D::RegisterObject(Context* context)
{
    context->RegisterFactory<AnimationSet2D>();
}

bool AnimationSet2D::BeginLoad(Deserializer& source)
{
    Dispose();

    if (GetName().Empty())
        SetName(source.GetName());

    String extension = GetExtension(source.GetName());
#ifdef URHO3D_SPINE
    if (extension == ".json")
        return BeginLoadSpine(source);
#endif
    if (extension == ".scml")
        return BeginLoadSpriter(source);

    URHO3D_LOGERROR("Unsupport animation set file: " + source.GetName());

    return false;
}

bool AnimationSet2D::EndLoad()
{
#ifdef URHO3D_SPINE
    if (jsonData_)
        return EndLoadSpine();
#endif
    if (spriterData_)
        return EndLoadSpriter();

    return false;
}

unsigned AnimationSet2D::GetNumAnimations() const
{
#ifdef URHO3D_SPINE
    if (skeletonData_)
        return (unsigned)skeletonData_->animationsCount;
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return (unsigned)spriterData_->entities_[0]->animations_.Size();
    return 0;
}

String AnimationSet2D::GetAnimation(unsigned index) const
{
    if (index >= GetNumAnimations())
        return String::EMPTY;

#ifdef URHO3D_SPINE
    if (skeletonData_)
        return skeletonData_->animations[index]->name;
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
        return spriterData_->entities_[0]->animations_[index]->name_;

    return String::EMPTY;
}

bool AnimationSet2D::HasAnimation(const String& animationName) const
{
#ifdef URHO3D_SPINE
    if (skeletonData_)
    {
        for (int i = 0; i < skeletonData_->animationsCount; ++i)
        {
            if (animationName == skeletonData_->animations[i]->name)
                return true;
        }
    }
#endif
    if (spriterData_ && !spriterData_->entities_.Empty())
    {
        const PODVector<Spriter::Animation*>& animations = spriterData_->entities_[0]->animations_;
        for (unsigned i = 0; i < animations.Size(); ++i)
        {
            if (animationName == animations[i]->name_)
                return true;
        }
    }

    return false;
}

Sprite2D* AnimationSet2D::GetSpriterFileSprite(int folderId, int fileId) const
{
    int key = (folderId << 16) + fileId;
    HashMap<int, SharedPtr<Sprite2D> >::ConstIterator i = spriterFileSprites_.Find(key);
    if (i != spriterFileSprites_.End())
        return i->second_;

    return 0;
}

#ifdef URHO3D_SPINE
bool AnimationSet2D::BeginLoadSpine(Deserializer& source)
{
    if (GetName().Empty())
        SetName(source.GetName());

    unsigned size = source.GetSize();
    jsonData_ = new char[size + 1];
    source.Read(jsonData_, size);
    jsonData_[size] = '\0';
    SetMemoryUse(size);
    return true;
}

bool AnimationSet2D::EndLoadSpine()
{
    currentAnimationSet = this;

    String atlasFileName = ReplaceExtension(GetName(), ".atlas");
    atlas_ = spAtlas_createFromFile(atlasFileName.CString(), 0);
    if (!atlas_)
    {
        URHO3D_LOGERROR("Create spine atlas failed");
        return false;
    }

    int numAtlasPages = 0;
    spAtlasPage* atlasPage = atlas_->pages;
    while (atlasPage)
    {
        ++numAtlasPages;
        atlasPage = atlasPage->next;
    }

    if (numAtlasPages > 1)
    {
        URHO3D_LOGERROR("Only one page is supported in Urho3D");
        return false;
    }

    sprite_ = static_cast<Sprite2D*>(atlas_->pages->rendererObject);

    spSkeletonJson* skeletonJson = spSkeletonJson_create(atlas_);
    if (!skeletonJson)
    {
        URHO3D_LOGERROR("Create skeleton Json failed");
        return false;
    }

    skeletonJson->scale = 0.01f; // PIXEL_SIZE;
    skeletonData_ = spSkeletonJson_readSkeletonData(skeletonJson, &jsonData_[0]);

    spSkeletonJson_dispose(skeletonJson);
    jsonData_.Reset();

    currentAnimationSet = 0;

    return true;
}
#endif

bool AnimationSet2D::BeginLoadSpriter(Deserializer& source)
{
    unsigned dataSize = source.GetSize();
    if (!dataSize && !source.GetName().Empty())
    {
        URHO3D_LOGERROR("Zero sized XML data in " + source.GetName());
        return false;
    }

    SharedArrayPtr<char> buffer(new char[dataSize]);
    if (source.Read(buffer.Get(), dataSize) != dataSize)
        return false;

    spriterData_ = new Spriter::SpriterData();
    if (!spriterData_->Load(buffer.Get(), dataSize))
    {
        URHO3D_LOGERROR("Could not spriter data from " + source.GetName());
        return false;
    }

    // Check has sprite sheet
    String parentPath = GetParentPath(GetName());
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    spriteSheetFilePath_ = parentPath + GetFileName(GetName()) + ".xml";
    hasSpriteSheet_ = cache->Exists(spriteSheetFilePath_);
    if (!hasSpriteSheet_)
    {
        spriteSheetFilePath_ = parentPath + GetFileName(GetName()) + ".plist";
        hasSpriteSheet_ = cache->Exists(spriteSheetFilePath_);
    }

    if (GetAsyncLoadState() == ASYNC_LOADING)
    {
        if (hasSpriteSheet_)
            cache->BackgroundLoadResource<SpriteSheet2D>(spriteSheetFilePath_, true, this);
        else
        {
            for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
            {
                Spriter::Folder* folder = spriterData_->folders_[i];
                for (unsigned j = 0; j < folder->files_.Size(); ++j)
                {
                    Spriter::File* file = folder->files_[j];
                    String imagePath = parentPath + file->name_;
                    cache->BackgroundLoadResource<Image>(imagePath, true, this);
                }
            }
        }
    }

    // Note: this probably does not reflect internal data structure size accurately
    SetMemoryUse(dataSize);

    return true;
}

struct SpriteInfo
{
    int x;
    int y;
    Spriter::File* file_;
    SharedPtr<Image> image_;
};

bool AnimationSet2D::EndLoadSpriter()
{
    if (!spriterData_)
        return false;

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    if (hasSpriteSheet_)
    {
        spriteSheet_ = cache->GetResource<SpriteSheet2D>(spriteSheetFilePath_);
        if (!spriteSheet_)
            return false;

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                SharedPtr<Sprite2D> sprite(spriteSheet_->GetSprite(GetFileName(file->name_)));
                if (!sprite)
                {
                    URHO3D_LOGERROR("Could not load sprite " + file->name_);
                    return false;
                }

                Vector2 hotSpot(file->pivotX_, file->pivotY_);

                // If sprite is trimmed, recalculate hot spot
                const IntVector2& offset = sprite->GetOffset();
                if (offset != IntVector2::ZERO)
                {
                    float pivotX = file->width_ * hotSpot.x_;
                    float pivotY = file->height_ * (1.0f - hotSpot.y_);

                    const IntRect& rectangle = sprite->GetRectangle();
                    hotSpot.x_ = (offset.x_ + pivotX) / rectangle.Width();
                    hotSpot.y_ = 1.0f - (offset.y_ + pivotY) / rectangle.Height();
                }

                sprite->SetHotSpot(hotSpot);

                if (!sprite_)
                    sprite_ = sprite;

                int key = (folder->id_ << 16) + file->id_;
                spriterFileSprites_[key] = sprite;
            }
        }
    }
    else
    {
        Vector<SpriteInfo> spriteInfos;
        String parentPath = GetParentPath(GetName());

        for (unsigned i = 0; i < spriterData_->folders_.Size(); ++i)
        {
            Spriter::Folder* folder = spriterData_->folders_[i];
            for (unsigned j = 0; j < folder->files_.Size(); ++j)
            {
                Spriter::File* file = folder->files_[j];
                String imagePath = parentPath + file->name_;
                SharedPtr<Image> image(cache->GetResource<Image>(imagePath));
                if (!image)
                {
                    URHO3D_LOGERROR("Could not load image");
                    return false;
                }
                if (image->IsCompressed())
                {
                    URHO3D_LOGERROR("Compressed image is not support");
                    return false;
                }
                if (image->GetComponents() != 4)
                {
                    URHO3D_LOGERROR("Only support image with 4 components");
                    return false;
                }

                SpriteInfo def;
                def.x = 0;
                def.y = 0;
                def.file_ = file;
                def.image_ = image;
                spriteInfos.Push(def);
            }
        }

        if (spriteInfos.Empty())
            return false;

        if (spriteInfos.Size() > 1)
        {
            AreaAllocator allocator(128, 128, 2048, 2048);
            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriteInfo& info = spriteInfos[i];
                Image* image = info.image_;
                if (!allocator.Allocate(image->GetWidth() + 1, image->GetHeight() + 1, info.x, info.y))
                {
                    URHO3D_LOGERROR("Could not allocate area");
                    return false;
                }
            }

            SharedPtr<Texture2D> texture(new Texture2D(context_));
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);
            texture->SetSize(allocator.GetWidth(), allocator.GetHeight(), Graphics::GetRGBAFormat());

            unsigned textureDataSize = allocator.GetWidth() * allocator.GetHeight() * 4;
            SharedArrayPtr<unsigned char> textureData(new unsigned char[textureDataSize]);
            memset(textureData.Get(), 0, textureDataSize);

            sprite_ = new Sprite2D(context_);
            sprite_->SetTexture(texture);

            for (unsigned i = 0; i < spriteInfos.Size(); ++i)
            {
                SpriteInfo& info = spriteInfos[i];
                Image* image = info.image_;

                for (int y = 0; y < image->GetHeight(); ++y)
                {
                    memcpy(textureData.Get() + ((info.y + y) * allocator.GetWidth() + info.x) * 4,
                        image->GetData() + y * image->GetWidth() * 4, image->GetWidth() * 4);
                }

                SharedPtr<Sprite2D> sprite(new Sprite2D(context_));
                sprite->SetTexture(texture);
                sprite->SetRectangle(IntRect(info.x, info.y, info.x + image->GetWidth(), info.y + image->GetHeight()));
                sprite->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

                int key = (info.file_->folder_->id_ << 16) + info.file_->id_;
                spriterFileSprites_[key] = sprite;
            }

            texture->SetData(0, 0, 0, allocator.GetWidth(), allocator.GetHeight(), textureData.Get());
        }
        else
        {
            SharedPtr<Texture2D> texture(new Texture2D(context_));
            texture->SetMipsToSkip(QUALITY_LOW, 0);
            texture->SetNumLevels(1);

            SpriteInfo& info = spriteInfos[0];
            texture->SetData(info.image_, true);

            sprite_ = new Sprite2D(context_);
            sprite_->SetTexture(texture);
            sprite_->SetRectangle(IntRect(info.x, info.y, info.x + info.image_->GetWidth(), info.y + info.image_->GetHeight()));
            sprite_->SetHotSpot(Vector2(info.file_->pivotX_, info.file_->pivotY_));

            int key = (info.file_->folder_->id_ << 16) + info.file_->id_;
            spriterFileSprites_[key] = sprite_;
        }
    }

    return true;
}

void AnimationSet2D::Dispose()
{
#ifdef URHO3D_SPINE
    if (skeletonData_)
    {
        spSkeletonData_dispose(skeletonData_);
        skeletonData_ = 0;
    }

    if (atlas_)
    {
        spAtlas_dispose(atlas_);
        atlas_ = 0;
    }
#endif

    spriterData_.Reset();

    sprite_.Reset();
    spriteSheet_.Reset();
    spriterFileSprites_.Clear();
}

bool AnimationSet2D::Save(Serializer& dest) const
{
    XMLFile xmlFile(context_);

    XMLElement rootElem = xmlFile.CreateRoot("spriter_data");
    if (!SaveXML(rootElem))
        return false;

    return xmlFile.Save(dest);
}

bool AnimationSet2D::SaveXML(XMLElement& dest) const
{
	// Header
	dest.SetInt("scml_version", spriterData_->scmlVersion_);
	dest.SetAttribute("generator", spriterData_->generator_);
	dest.SetAttribute("generator_version", spriterData_->generatorVersion_);

	// Folders
	const PODVector<Spriter::Folder*>& folders = spriterData_->folders_;
	for (size_t i = 0; i < folders.Size(); ++i)
	{
		XMLElement folderElem = dest.CreateChild("folder");
		Spriter::Folder* folder = folders[i];
		folderElem.SetInt("id", folder->id_);
		if (!folder->name_.Empty())
			folderElem.SetAttribute("name", folder->name_);

		// Files
		const PODVector<Spriter::File*>& files = folder->files_;
		for (size_t f = 0; f < files.Size(); ++f)
		{
			XMLElement fileElem = folderElem.CreateChild("file");
			Spriter::File* file = files[f];
			fileElem.SetInt("id", file->id_);
			fileElem.SetAttribute("name", file->name_);
			fileElem.SetFloat("width", file->width_);
			fileElem.SetFloat("height", file->height_);
			fileElem.SetFloat("pivot_x", file->pivotX_);
			fileElem.SetFloat("pivot_y", file->pivotY_);
		}
	}

	// Entities (note that obj_info is discarded)
	const PODVector<Spriter::Entity*>& entities = spriterData_->entities_;
	for (size_t e = 0; e < entities.Size(); ++e)
    {
		XMLElement entityElem = dest.CreateChild("entity");
		Spriter::Entity* entity = entities[e];
		entityElem.SetInt("id", entity->id_);
		entityElem.SetAttribute("name", entity->name_);

		// Character maps
		const PODVector<Spriter::CharacterMap*>& characterMaps = entity->characterMaps_;
		for (size_t cm = 0; cm < characterMaps.Size(); ++cm)
		{
			XMLElement characterMapElem = entityElem.CreateChild("character_map");
			Spriter::CharacterMap* characterMap = characterMaps[cm];
			characterMapElem.SetInt("id", characterMap->id_);
			characterMapElem.SetAttribute("name", characterMap->name_);

			const PODVector<Spriter::MapInstruction*>& maps = characterMap->maps_;
			for (size_t mi = 0; mi < characterMaps.Size(); ++mi)
			{
				XMLElement mapElem = characterMapElem.CreateChild("character_map");
				Spriter::MapInstruction* map = maps[mi];
				mapElem.SetInt("folder", map->folder_);
				mapElem.SetInt("file", map->file_);
				mapElem.SetInt("target_folder", map->targetFolder_);
				mapElem.SetInt("target_file", map->targetFile_);
			}
		}

		// Animations
		const PODVector<Spriter::Animation*>& animations = entity->animations_;
		for (size_t a = 0; a < animations.Size(); ++a)
		{
			XMLElement animationElem = entityElem.CreateChild("animation");
			Spriter::Animation* animation = animations[a];
			animationElem.SetInt("id", animation->id_);
			animationElem.SetAttribute("name", animation->name_);
			animationElem.SetFloat("length", animation->length_ * 1000.0f);
			if (!animation->looping_)
				animationElem.SetBool("looping", animation->looping_);

			// Main line keys (note that some object_ref settings are discarded)
			XMLElement mainlineElem = animationElem.CreateChild("mainline");
			const PODVector<Spriter::MainlineKey*>& mainlineKeys = animation->mainlineKeys_;
			for (size_t m = 0; m < mainlineKeys.Size(); ++m)
			{
				XMLElement keyElem = mainlineElem.CreateChild("key");
				Spriter::MainlineKey* mainlineKey = mainlineKeys[m];
				keyElem.SetInt("id", mainlineKey->id_);
				if (mainlineKey->time_ > 0.0f)
					keyElem.SetFloat("time", mainlineKey->time_ * 1000.0f);

				// Bone refs
				const PODVector<Spriter::Ref*>& boneRefs = mainlineKey->boneRefs_;
				for (size_t b = 0; b < boneRefs.Size(); ++b)
				{
					XMLElement boneRefsElem = keyElem.CreateChild("bone_ref");
					Spriter::Ref* boneRef = boneRefs[b];
					boneRefsElem.SetInt("id", boneRef->id_);
					if (boneRef->parent_ >= 0)
						boneRefsElem.SetInt("parent", boneRef->parent_);
					boneRefsElem.SetInt("timeline", boneRef->timeline_);
					boneRefsElem.SetInt("key", boneRef->key_);
				}

				// Object refs
				const PODVector<Spriter::Ref*>& objectRefs = mainlineKey->objectRefs_;
				for (size_t o = 0; o < objectRefs.Size(); ++o)
				{
					XMLElement objectRefsElem = keyElem.CreateChild("object_ref");
					Spriter::Ref* objectRef = objectRefs[o];
					objectRefsElem.SetInt("id", objectRef->id_);
					if (objectRef->parent_ >= 0)
						objectRefsElem.SetInt("parent", objectRef->parent_);
					objectRefsElem.SetInt("timeline", objectRef->timeline_);
					objectRefsElem.SetInt("key", objectRef->key_);
					objectRefsElem.SetInt("z_index", objectRef->zIndex_);
				}
			}

			// Timelines
			const PODVector<Spriter::Timeline*>& timelines = animation->timelines_;
			for (size_t t = 0; t < timelines.Size(); ++t)
			{
				XMLElement timelineElem = animationElem.CreateChild("timeline");
				Spriter::Timeline* timeline = animation->timelines_[t];
				timelineElem.SetInt("id", timeline->id_);
				timelineElem.SetAttribute("name", timeline->name_);
				if (timeline->objectType_ == Spriter::BONE)
					timelineElem.SetAttribute("object_type", "bone");

				// Keys
				const PODVector<Spriter::SpatialTimelineKey*>& keys = timeline->keys_;
				for (size_t i = 0; i < keys.Size(); ++i)
				{
					XMLElement keyElem = timelineElem.CreateChild("key");
					Spriter::SpatialTimelineKey* spatialKey = keys[i];
					const Spriter::SpatialInfo& info = spatialKey->info_;
					keyElem.SetInt("id", spatialKey->id_);
					if (spatialKey->time_ > 0.0f)
						keyElem.SetFloat("time", spatialKey->time_ * 1000.0f);
					if (info.spin != 1)
						keyElem.SetInt("spin", info.spin);

					XMLElement elt;
					if (timeline->objectType_ == Spriter::SPRITE)
					{
						elt = keyElem.CreateChild("object");
						Spriter::SpriteTimelineKey* spriteKey = (Spriter::SpriteTimelineKey*)(spatialKey);
						elt.SetInt("folder", spriteKey->folderId_);
						elt.SetInt("file", spriteKey->fileId_);
						elt.SetBool("useDefaultPivot", spriteKey->useDefaultPivot_); // True if missing pivot_x and pivot_y in object tag
						if (spriteKey->pivotX_ != 0.0f)
							elt.SetFloat("pivot_x", spriteKey->pivotX_);
						if (spriteKey->pivotY_ != 0.0f)
							elt.SetFloat("pivot_y", spriteKey->pivotY_);
					}
					else if (timeline->objectType_ == Spriter::BONE)
					{
						elt = keyElem.CreateChild("bone");
						Spriter::BoneTimelineKey* boneKey = (Spriter::BoneTimelineKey*)(spatialKey);
						elt.SetFloat("w", boneKey->length_); // Unimplemented in Spriter
						elt.SetFloat("h", boneKey->width_); // Unimplemented in Spriter
					}

					if (info.x_ != 0.0f)
						elt.SetFloat("x", info.x_);
					if (info.y_ != 0.0f)
						elt.SetFloat("y", info.y_);
					elt.SetFloat("angle", info.angle_);
					if (info.scaleX_ != 1.0f)
						elt.SetFloat("scale_x", info.scaleX_);
					if (info.scaleY_ != 1.0f)
						elt.SetFloat("scale_y", info.scaleY_);
					if (info.alpha_ != 1.0f)
						elt.SetFloat("a", info.alpha_);
				}
			}
		}
	}
}

}
