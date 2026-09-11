import logging
from typing import Any

import iceoryx2 as iox2

from .core.rvc_inference import RVCInference
from .ipc.handlers.base_handler import BaseHandler
from .ipc.handlers.rvc_handler import RVCHandler
from .ipc.handlers.settings_handler import SettingsHandler
from .settings import settings

logger: logging.Logger = logging.getLogger(__name__)


def main() -> None:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    rvc: RVCInference = RVCInference()
    handlers: tuple[BaseHandler[Any, Any], ...] = (RVCHandler(rvc), SettingsHandler(rvc))
    node: iox2.Node = iox2.NodeBuilder.new().create(iox2.ServiceType.Ipc)
    servers: tuple[iox2.Server, ...] = tuple(handler.register(node) for handler in handlers)
    logger.info(
        "OBS RVC worker listening on iceoryx2 services %s",
        ", ".join(handler.service_name for handler in handlers),
    )

    try:
        while True:
            node.wait(iox2.Duration.from_millis(settings.wait_ms))
            for handler, server in zip(handlers, servers):
                while handler.process(server):
                    pass
    except (iox2.NodeWaitFailure, KeyboardInterrupt):
        logger.info("Stopping OBS RVC worker")
    finally:
        for server in servers:
            server.delete()


if __name__ == "__main__":
    main()
