import logging

import iceoryx2 as iox2

from .core.rvc_inference import RVCInference
from .ipc.handlers.rvc_handler import RVCHandler
from .ipc.publishers.audio_publisher import AudioPublisher
from .schemas.requests.audio_request_schema import AudioRequestSchema
from .schemas.responses.audio_response_schema import AudioResponseSchema
from .ipc.subscribers.audio_subscriber import AudioSubscriber
from .settings import settings

logger = logging.getLogger(__name__)


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    handler: RVCHandler = RVCHandler(RVCInference())
    node: iox2.Node = iox2.NodeBuilder.new().create(iox2.ServiceType.Ipc)
    server: iox2.Server = (
        node.service_builder(iox2.ServiceName.new(settings.service_name))
        .request_response(AudioRequestSchema, AudioResponseSchema)
        .open_or_create()
        .server_builder()
        .create()
    )

    logger.info("OBS RVC worker listening on iceoryx2 service %s", settings.service_name)
    try:
        while True:
            node.wait(iox2.Duration.from_millis(settings.wait_ms))
            while True:
                request = AudioSubscriber.receive(server)
                if request is None:
                    break
                try:
                    AudioPublisher.publish(
                        request,
                        handler.handle(request.payload().contents),
                    )
                finally:
                    request.delete()
    except (iox2.NodeWaitFailure, KeyboardInterrupt):
        logger.info("Stopping OBS RVC worker")
    finally:
        server.delete()


if __name__ == "__main__":
    main()
