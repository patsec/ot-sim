/************************************************************************
* Copyright (c) 2018, Sandia National Laboratories                      *
* All rights reserved.                                                  *
*                                                                       *
* This computer software was prepared by National Technology &          *
* Engineering Solutions of Sandia, LLC, hereinafter the Contractor,     *
* under Contract DE-NA0003525 with the Department of Energy (DOE). All  *
* rights in the computer software are reserved by DOE on behalf of the  *
* United States Government and the Contractor as provided in the        *
* Contract. You are authorized to use this computer software for        *
* Governmental purposes but it is not to be released or distributed to  *
* the public. NEITHER THE GOVERNMENT NOR THE CONTRACTOR MAKES ANY       *
* WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LIABILITY FOR THE USE OF *
* THIS SOFTWARE. This notice including this sentence must appear on any *
* copies of this computer software.                                     *
************************************************************************/

package sigterm

import (
	"context"
	"os"
	"os/signal"
	"syscall"
)

func CancelContext(ctx context.Context) context.Context {
	ctxWithCancel, cancel := context.WithCancel(ctx)

	go func() {
		defer cancel()

		term := make(chan os.Signal, 1)
		signal.Notify(term, syscall.SIGTERM, syscall.SIGINT)

		select {
		case <-term:
		case <-ctx.Done():
		}
	}()

	return ctxWithCancel
}
