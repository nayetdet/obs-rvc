import logging

import iceoryx2 as iox2

from .core.rvc.rvc_inference import RVCInference
from .endpoints.rvc_endpoint import RVCEndpoint
from .endpoints.settings_endpoint import SettingsEndpoint
from .settings import settings

logger: logging.Logger = logging.getLogger(__name__)


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    rvc: RVCInference = RVCInference()
    endpoints = (RVCEndpoint(rvc), SettingsEndpoint(rvc))
    node: iox2.Node = iox2.NodeBuilder.new().create(iox2.ServiceType.Ipc)
    servers: tuple[iox2.Server, ...] = tuple(endpoint.register(node) for endpoint in endpoints)
    logger.info(
        "OBS RVC worker listening on iceoryx2 services %s",
        ", ".join(endpoint.service_name for endpoint in endpoints),
    )

    try:
        while True:
            node.wait(iox2.Duration.from_millis(settings.wait_ms))
            for endpoint, server in zip(endpoints, servers):
                while endpoint.process(server):
                    pass
    except (iox2.NodeWaitFailure, KeyboardInterrupt):
        logger.info("Stopping OBS RVC worker")
    finally:
        endpoints[0].close()
        for server in servers:
            server.delete()


if __name__ == "__main__":
    main()
