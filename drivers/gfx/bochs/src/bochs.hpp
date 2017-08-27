
#include <queue>
#include <unordered_map>

#include <arch/mem_space.hpp>
#include <async/doorbell.hpp>
#include <async/mutex.hpp>
#include <async/result.hpp>

#include "spec.hpp"

// ----------------------------------------------------------------
// Sequential ID allocator
// ----------------------------------------------------------------

#include <assert.h>
#include <limits>
#include <set>

// Allocator for integral IDs. Provides O(log n) allocation and deallocation.
// Allocation always returns the smallest available ID.
template<typename T>
struct id_allocator {
private:
	struct node {
		T lb;
		T ub;

		friend bool operator< (const node &u, const node &v) {
			return u.lb < v.lb;
		}
	};

public:
	id_allocator(T lb = 1, T ub = std::numeric_limits<T>::max()) {
		_nodes.insert(node{lb, ub});
	}

	T allocate() {
		assert(!_nodes.empty());
		auto it = _nodes.begin();
		auto id = it->lb;
		if(it->lb < it->ub)
			_nodes.insert(std::next(it), node{it->lb + 1, it->ub});
		_nodes.erase(it);
		return id;
	}

	void free(T id) {
		// TODO: We could coalesce multiple adjacent nodes here.
		_nodes.insert(node{id, id});
	}

private:
	std::set<node> _nodes;
};

// ----------------------------------------------------------------
// Stuff that belongs in a DRM library.
// ----------------------------------------------------------------
namespace drm_backend {

struct Crtc;
struct Encoder;
struct Connector;
struct Configuration;
struct FrameBuffer;
struct Plane;
struct Assignment;
struct Object;

struct Property {

};

struct BufferObject {
	virtual std::shared_ptr<BufferObject> sharedBufferObject() = 0;
};


struct Device {
	virtual std::unique_ptr<Configuration> createConfiguration() = 0;
	virtual std::shared_ptr<BufferObject> createDumb() = 0;
	virtual std::shared_ptr<FrameBuffer> createFrameBuffer(std::shared_ptr<BufferObject> buff) = 0;
	
	void setupCrtc(std::shared_ptr<Crtc> crtc);
	void setupEncoder(std::shared_ptr<Encoder> encoder);
	void attachConnector(std::shared_ptr<Connector> connector);
	const std::vector<std::shared_ptr<Crtc>> &getCrtcs();
	const std::vector<std::shared_ptr<Encoder>> &getEncoders();
	const std::vector<std::shared_ptr<Connector>> &getConnectors();
	
	void registerObject(std::shared_ptr<Object> object);
	Object *findObject(uint32_t);

private:	
	std::vector<std::shared_ptr<Crtc>> _crtcs;
	std::vector<std::shared_ptr<Encoder>> _encoders;
	std::vector<std::shared_ptr<Connector>> _connectors;
	std::unordered_map<uint32_t, std::shared_ptr<Object>> _objects;

public:
	id_allocator<uint32_t> allocator;
	Property srcWProperty;
	Property srcHProperty;
};

struct File {
	File(std::shared_ptr<Device> device)
		:_device(device) { };
	static async::result<size_t> read(std::shared_ptr<void> object, void *buffer, size_t length);
	static async::result<protocols::fs::AccessMemoryResult> accessMemory(std::shared_ptr<void> object, uint64_t, size_t);
	static async::result<void> ioctl(std::shared_ptr<void> object, managarm::fs::CntRequest req,
			helix::UniqueLane conversation);

	void attachFrameBuffer(std::shared_ptr<FrameBuffer> frame_buffer);
	const std::vector<std::shared_ptr<FrameBuffer>> &getFrameBuffers();
	
	uint32_t createHandle(std::shared_ptr<BufferObject> buff);
	BufferObject *resolveHandle(uint32_t handle);

private:
	std::vector<std::shared_ptr<FrameBuffer>> _frameBuffers;
	std::shared_ptr<Device> _device;
	std::unordered_map<uint32_t, std::shared_ptr<BufferObject>> _buffers;
	id_allocator<uint32_t> _allocator;
	
};

struct Configuration {
	virtual bool capture(std::vector<Assignment> assignment) = 0;
	virtual void dispose() = 0;
	virtual void commit() = 0;
};

struct Crtc {
	virtual Object *asObject() = 0;
	virtual Plane *primaryPlane() = 0;
};

struct Encoder {
	Encoder()
		:_currentCrtc(nullptr) {  };
	
	virtual Object *asObject() = 0;	
	drm_backend::Crtc *currentCrtc();
	void setCurrentCrtc(drm_backend::Crtc *crtc);

private:
	drm_backend::Crtc *_currentCrtc;
};

struct Connector {
	virtual const std::vector<Encoder *> &possibleEncoders() = 0;
	virtual Object *asObject() = 0;
};

struct FrameBuffer {
	virtual Object *asObject() = 0;
};

struct Plane {
	virtual Object *asObject() = 0;
};

struct Assignment {
	Object *object;
	Property *property;
	uint64_t intValue;
};

struct Object {
	Object(uint32_t id)
		:_id(id) { };
	
	uint32_t id();
	virtual Encoder *asEncoder();
	virtual Connector *asConnector();
	virtual Crtc *asCrtc();
	virtual FrameBuffer *asFrameBuffer();
	virtual Plane *asPlane();
	
private:
	uint32_t _id;
};

}

// ----------------------------------------------------------------

struct GfxDevice : drm_backend::Device, std::enable_shared_from_this<GfxDevice> {
	struct Configuration : drm_backend::Configuration {
		Configuration(GfxDevice *device)
		:_device(device), _width(0), _height(0) { };
		
		bool capture(std::vector<drm_backend::Assignment> assignment) override;
		void dispose() override;
		void commit() override;
		
	private:
		GfxDevice *_device;
		int _width;
		int _height;
	};

	struct Plane : drm_backend::Object, drm_backend::Plane {
		Plane(GfxDevice *device);
		
		drm_backend::Plane *asPlane() override;	
		drm_backend::Object *asObject() override;
	};
	
	struct BufferObject : drm_backend::BufferObject, std::enable_shared_from_this<BufferObject> {
		std::shared_ptr<drm_backend::BufferObject> sharedBufferObject() override;
	};

	struct Connector : drm_backend::Object, drm_backend::Connector {
		Connector(GfxDevice *device);
		
		drm_backend::Connector *asConnector() override;
		drm_backend::Object *asObject() override;
		const std::vector<drm_backend::Encoder *> &possibleEncoders() override;

	private:
		std::vector<drm_backend::Encoder *> _encoders;
	};

	struct Encoder : drm_backend::Object, drm_backend::Encoder {
		Encoder(GfxDevice *device);
		
		drm_backend::Encoder *asEncoder() override;
		drm_backend::Object *asObject() override;
	};
	
	struct Crtc : drm_backend::Object, drm_backend::Crtc {
		Crtc(GfxDevice *device);
		
		drm_backend::Crtc *asCrtc() override;
		drm_backend::Object *asObject() override;
		drm_backend::Plane *primaryPlane() override;
	
	private:	
		GfxDevice *_device;
	};

	struct FrameBuffer : drm_backend::Object, drm_backend::FrameBuffer {
		FrameBuffer(GfxDevice *device);

		drm_backend::FrameBuffer *asFrameBuffer() override;
		drm_backend::Object *asObject() override;
	};

	GfxDevice(helix::UniqueDescriptor video_ram, void* frame_buffer);
	
	cofiber::no_future initialize();
	std::unique_ptr<drm_backend::Configuration> createConfiguration() override;
	std::shared_ptr<drm_backend::BufferObject> createDumb() override;
	std::shared_ptr<drm_backend::FrameBuffer> 
			createFrameBuffer(std::shared_ptr<drm_backend::BufferObject> buff) override;

private:
	std::shared_ptr<Crtc> _theCrtc;
	std::shared_ptr<Encoder> _theEncoder;
	std::shared_ptr<Connector> _theConnector;
	std::shared_ptr<Plane> _primaryPlane;

public:
	// FIX ME: this is a hack	
	helix::UniqueDescriptor _videoRam;
private:
	arch::io_space _operational;
	void* _frameBuffer;
};
